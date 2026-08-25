//
//  ViewExport.swift
//  ChartRender
//
//  Created by Leptos on 8/25/26.
//

import SwiftUI
import UniformTypeIdentifiers

enum ViewExport {
    static func render(content: some View, size: CGSize, environment: EnvironmentValues, baseName: String) {
        let renderer = ImageRenderer(
            content: content
                .environment(\.self, environment)
        )
        renderer.proposedSize = .init(width: size.width, height: size.height)
        renderer.scale = 4
        renderer.colorMode = .nonLinear
        renderer.isObservationEnabled = false
        guard let cgImage = renderer.cgImage else {
            fatalError()
        }

        guard let rendersDir = ProcessInfo.processInfo.environment["RENDERS_DIR"] else {
            fatalError("RENDERS_DIR not set")
        }
        let directoryURL = URL(fileURLWithPath: rendersDir, isDirectory: true)
        try! FileManager.default.createDirectory(at: directoryURL, withIntermediateDirectories: true)

        let colorScheme = environment.colorScheme == .dark ? "dark" : "light"
        let fileBaseName: String = "\(baseName)-\(colorScheme)"

        let outputFormats: [UTType] = [
            .png,
            .init(filenameExtension: "avif", conformingTo: .image)!
        ]

        for utType in outputFormats {
            let fileURL = directoryURL
                .appendingPathComponent(fileBaseName)
                .appendingPathExtension(for: utType)
            guard let destination = CGImageDestinationCreateWithURL(fileURL as CFURL, utType.identifier as CFString, 1, nil) else {
                fatalError()
            }
            let options: [CFString: Any] = [
                kCGImageDestinationOptimizeColorForSharing: true,
            ]
            CGImageDestinationAddImage(destination, cgImage, options as CFDictionary)
            guard CGImageDestinationFinalize(destination) else {
                fatalError()
            }
        }
    }
}
