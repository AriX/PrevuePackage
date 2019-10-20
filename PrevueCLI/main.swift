//
//  main.swift
//  PrevueCLI
//
//  Created by Ari on 11/17/18.
//  Copyright © 2018 Vertex. All rights reserved.
//

import Foundation

let destination = SerialPortDataDestination(path: "/dev/cu.usbserial")//FSUAEDataDestination(host: "127.0.0.1", port: 5542)
destination.openConnection()

destination.send(BoxOnCommand(selectCode: "*"))

// Configuration test
//let command2 = ConfigurationCommand(timeslotsBack: 1, timeslotsForward: 4, scrollSpeed: 3, maxAdCount: 36, maxAdLines: 6, unknown: false, unknownAdSetting: 0x0101, timezone: 7, observesDaylightSavingsTime: true, cont: true, keyboardActive: false, unknown2: false, unknown3: false, unknown4: true, unknown5: 0x41, grph: 0x4E, videoInsertion: 0x4E, unknown6: 0x00)
//destination.send(command2)

// Download test
//var bytes: Bytes = []
//for _ in stride(from: 0, to: 256, by: 1) {
//    bytes.append(0x30)
//}
//let downloadCommands = DownloadCommand.commandsToTransferFile(filePath: "DF0:saladworks2.fun", contents: bytes)
//destination.send(downloadCommands)
//destination.send(BoxOffCommand())

// Config.dat test
//destination.send(ConfigDatCommand(clockCmd: 2).encodeWithChecksum())

// Channel & programs test

let date = Date()
let julianDay = JulianDay(dayOfYear: JulianDay(with: date).dayOfYear - 1)

destination.send(ClockCommand(with: date)!)

let channelsFile = URL(fileURLWithPath: "/Users/Ari/Desktop/Prevue Technical/Sample Listings/channels.csv")
let programsFile = URL(fileURLWithPath: "/Users/Ari/Desktop/Prevue Technical/Sample Listings/programs.csv")
let listingSource = SampleDataListingSource(channelsCSVFile: channelsFile, programsCSVFile: programsFile, day: julianDay)!
//
//let channels = [Channel(flags: .none, sourceIdentifier: "TBS", channelNumber: "2", callLetters: "TBS"), Channel(flags: .none, sourceIdentifier: "KYW", channelNumber: "3", callLetters: "KYW")]
let channelCommand = ChannelsCommand(day: julianDay, channels: listingSource.channels)
destination.send(channelCommand)

//let programs: [Program] = stride(from: 0, to: 47, by: 1).map { (index) in
//    Program(timeslot: index, day: julianDay, sourceIdentifier: "KYW", flags: .none, programName: "Eyewitness @ \(index) \(julianDay.dayOfYear)")
//}
let programCommands: [ProgramCommand] = listingSource.programs.map { (program) in
    ProgramCommand(program: program)
}
destination.send(programCommands)

// Clock test
//destination.send(ClockCommand(dayOfWeek: .Friday, month: 2, day: 4, year: 119, hour: 07, minute: 00, second: 00, daylightSavingsTime: true))
//destination.send(ClockCommand(with: Date())!)

destination.send(BoxOffCommand())

destination.closeConnection()
