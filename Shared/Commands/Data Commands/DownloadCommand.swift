//
//  DownloadCommand.swift
//  PrevueFramework
//
//  Created by Ari on 11/17/18.
//  Copyright © 2018 Vertex. All rights reserved.
//

// Sources: http://ariweinstein.com/prevue/viewtopic.php?p=1413#p1413, UVSG test files

struct DownloadCommand: DataCommand {
    let commandMode = DataCommandMode.download
    enum Message {
        case start(filePath: String)
        case data(packetNumber: UInt8, byteCount: UInt8, data: Bytes)
        case end(packetCount: UInt8)
    }
    let message: Message
}

extension DownloadCommand {
    var payload: Bytes {
        switch message {
        case .start(let filePath):
            return filePath.uvsgBytes()
        case .data(let packetNumber, let byteCount, let data):
            return [packetNumber] + [byteCount] + data
        case .end(let packetCount):
            return [packetCount, 0x00]
        }
    }
}

extension DownloadCommand {
    static func commandsToTransferFile(filePath: String, contents: Bytes) -> [DownloadCommand] {
        let maxPacketSize = Int(Byte.max)
        let packets = contents.splitIntoChunks(chunkSize: maxPacketSize)
        
        let startCommand = DownloadCommand(message: .start(filePath: filePath))
        let packetCommands = packets.enumerated().map { index, packet in
            return DownloadCommand(message: .data(packetNumber: UInt8(index), byteCount: UInt8(packet.count), data: packet))
        }
        let endCommand = DownloadCommand(message: .end(packetCount: UInt8(packets.count)))
        return [startCommand] + packetCommands + [endCommand]
    }
}