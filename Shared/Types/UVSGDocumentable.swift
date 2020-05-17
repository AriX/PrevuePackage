//
//  UVSGDocumentable.swift
//  PrevueCLI
//
//  Created by Ari on 5/16/20.
//  Copyright © 2020 Vertex. All rights reserved.
//

import Foundation

// TODO: Support option sets

protocol UVSGDocumentable {
    var documentedType: UVSGDocumentedType { get }
}

protocol UVSGDocumentableEnum: UVSGDocumentable {
    var allCaseNames: [String] { get }
}

indirect enum UVSGDocumentedType: CustomStringConvertible {
    case none
    case scalar(String)
    case optional(UVSGDocumentedType)
    case `enum`(UVSGDocumentableEnum)
    case array(UVSGDocumentedType)
    case dictionary([(String, UVSGDocumentedType)])
    
    var description: String {
        return description(at: 0)
    }
    
    func description(at level: Int, optional: Bool = false, array: Bool = false, needsNewline: Bool = false) -> String {
        // The following implementation is a total spitball and could probably be improved significantly
        var strings: [String] = []
        var needNewline = needsNewline
        switch self {
        case .none:
            return ""
        case .optional(let documentedType):
            return documentedType.description(at: level, optional: true)
        case .scalar(let typeName):
            strings = [typeName]
        case .array(let documentedType):
            return documentedType.description(at: level, array: true)
        case .enum(let documentableEnum):
            strings = [documentableEnum.allCaseNames.joined(separator: ", ")]
        case .dictionary(let types):
            needNewline = true
            strings = types.compactMap { (arg0) -> String? in
                let (serializationKey, documentedType) = arg0
                let newLevel = level + 1
                switch documentedType {
                case .none:
                    return nil
                case .scalar(_):
                    return "\(serializationKey): \(documentedType.description(at: 0))"
                case .array(_):
                    return "\(serializationKey): \(documentedType.description(at: newLevel))"
                case .optional(let documentedType):
                    return "\(serializationKey): \(documentedType.description(at: newLevel, optional: true, needsNewline: true))"
                case .dictionary(let types):
                    if types.count == 0 {
                        return "\(serializationKey): (no arguments)"
                    } else {
                        return "\(serializationKey): \(documentedType.description(at: newLevel, needsNewline: true))"
                    }
                case .enum(_):
                    return "\(serializationKey): \(documentedType.description(at: newLevel, needsNewline: true))"
                }
            }
        }
        
        let spacedContent = strings.map {
            let spacing = String(repeating: "    ", count: level)
            return "\(spacing)\($0)"
        }.joined(separator: "\n")
        let optionalText = (optional ? "(optional)" : (array ? "(array)" : ""))
        let newlineText = needNewline ? "\n" : ""
        return "\(optionalText)\(newlineText)\(spacedContent)"
    }
}

extension UVSGDocumentable {
    var documentedType: UVSGDocumentedType {
        get {
            let mirror = Mirror(reflecting: self)
            
            var types: [(String, UVSGDocumentedType)] = []
            for (label, value) in mirror.children {
                guard let label = label else { continue }
                
                var introspectableValue = (value as? UVSGDocumentable)
                let typeOfValue = type(of: value)
                if typeOfValue is DataCommandMode.Type { continue }
                
                let shouldSkipOptional = Self.self is PrevueCommandFile.SerializedCommand.Type
                if shouldSkipOptional {
                    if case Optional<Any>.some(let unwrappedValue) = value,
                        let typedValue = unwrappedValue as? UVSGDocumentable {
                        introspectableValue = typedValue
                    }
                }
                
                types.append((label, introspectableValue?.documentedType ?? UVSGDocumentedType.scalar("\(typeOfValue)")))
            }
            return .dictionary(types)
        }
    }
}

extension UVSGDocumentableEnum {
    var documentedType: UVSGDocumentedType {
        return .enum(self)
    }
}

extension Array: UVSGDocumentable where Element: UVSGDocumentable {
    var documentedType: UVSGDocumentedType {
        guard let first = first else { return .none }
        return .array(first.documentedType)
    }
}

extension Optional: UVSGDocumentable where Wrapped: UVSGDocumentable {
    var documentedType: UVSGDocumentedType {
        get {
            switch self {
            case .none:
                return .none
            case .some(let introspectable):
                return .optional(introspectable.documentedType)
            }
        }
    }
}

extension PrevueCommandFile.SerializedCommand: UVSGDocumentable {
}
