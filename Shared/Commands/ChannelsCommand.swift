//
//  ChannelsCommand.swift
//  PrevueApp
//
//  Created by Ari on 4/6/19.
//  Copyright © 2019 Vertex. All rights reserved.
//

struct ChannelsCommand: DataCommand {
    let commandMode = DataCommandMode.channel
    let day: JulianDay
    let channels: [Channel]
}

extension ChannelsCommand {
    var payload: Bytes {
        let encodedChannels = channels.reduce([]) { encodedChannels, channel in
            encodedChannels + channel.encode()
        }
        return [day.dayOfYear] + encodedChannels + [0x00]
    }
}

extension Channel {
    func encode() -> Bytes {
        return [0x12, flags.rawValue] + sourceIdentifier.asBytes() + [0x11] + channelNumber.asBytes() + [0x01] + callLetters.asBytes()
    }
}
