//
//  DataDestination.swift
//  PrevuePackage
//
//  Created by Ari on 11/18/18.
//  Copyright © 2018 Vertex. All rights reserved.
//

import Foundation

/**
 A protocol describing an object which can receive data.
 */
protocol DataDestination {
    func send(data bytes: Bytes)
    func send(control bytes: Bytes)
}

/**
 Support sending UVSGEncodable commands to a data destination.
 */
// TODO: Should there instead be a single protocol for converting things into bytes, and this takes those?
extension DataDestination {
    func send(data command: UVSGEncodable) {
        send(data: command.encodeWithChecksum())
    }
    func send(data commands: [UVSGEncodable]) {
        var i = 0
        for command in commands {
            print("Sending \(i) of \(commands.count)")
            send(data: command)
            i += 1
        }
    }
    func send(control command: UVSGEncodable) {
        send(control: command.encodeWithChecksum())
    }
}

class NetworkDataDestination: DataDestination {
    var host: String
    var port: Int32
    
    init(host: String, port: Int32) {
        self.host = host
        self.port = port
    }
    
    func send(data bytes: Bytes) {
        print("Sending \(bytes.count) \(bytes.count == 1 ? "byte" : "bytes"): \(bytes.hexEncodedString())to \(self)")
    }
    
    func send(control bytes: Bytes) {
        // Unimplemented
    }
}

#if os(Windows)
import WinSDK
#endif

extension DataDestination {
    // Call this after sending data to rate limit sending to a particular baud rate
    func limitSendingRate(byteCount: Int, baudRate: Int) {
        // e.g. for 2400 baud, assume we're sending 240 bytes per second, with 1,000 miliseconds in a second
        // For unknown reasons, this seems to be too fast (for the Atari emulator) until I apply a factor of 4 or 5
        let timeToWait = (Double(byteCount) * 10.0) * (1 / Double(baudRate)) * 1000.0 * 4.5
        sleep(miliseconds: timeToWait)
    }
    
    func sleep(miliseconds: Double) {
        #if os(Windows)
        Sleep(UInt32(miliseconds))
        #else
        let microseconds = miliseconds * 1000.0
        usleep(UInt32(microseconds))
        #endif
    }
}
