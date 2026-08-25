//
//  HenView.swift
//  ChartRender
//
//  Created by Leptos on 8/23/26.
//

import SwiftUI
import Charts

struct HenView: View {
    static let previewSize: CGSize = .init(width: 720, height: 450)

    var isRender: Bool = false

    @Environment(\.self) private var fullEnvironment

    private static func eggs(for week: Int) -> Int {
        if week < 20 { return 0 }
        if week < 124 { return 5 }
        return 0
    }

    private static let graphRange: ClosedRange<Int> = 0...170
    private static let activeRange: ClosedRange<Int> = 0...170 // 52...170

    private static func totalEggCount() -> Int {
        activeRange.reduce(into: 0) { partialResult, week in
            partialResult += Self.eggs(for: week)
        }
    }

    private static func render(in environment: EnvironmentValues) {
        let rangeDescription: String = if activeRange.lowerBound <= graphRange.lowerBound {
            "full"
        } else {
            "\(activeRange.lowerBound)weeks"
        }

        ViewExport.render(
            content: HenView(isRender: true),
            size: Self.previewSize,
            environment: environment,
            baseName: "eggs_\(rangeDescription)"
        )
    }

    var body: some View {
        VStack(alignment: .leading) {
            if !isRender {
                HStack(alignment: .top) {
                    Spacer()

                    Button("Render") {
                        var environmentValues = fullEnvironment

                        environmentValues.colorScheme = .light
                        Self.render(in: environmentValues)

                        environmentValues.colorScheme = .dark
                        Self.render(in: environmentValues)
                    }

                    Text("Total eggs: \(Self.totalEggCount(), format: .number)")
                        .font(.footnote)
                        .padding(.trailing, 48)
                }
                .padding(.horizontal)
            }

            Chart {
                ForEach(Self.graphRange, id: \.self) { (week: Int) in
                    BarMark(
                        x: .value("Week", week),
                        yStart: .value("Floor", 0),
                        yEnd: .value("Count", Self.eggs(for: week)),
                        width: MarkDimension.fixed(3)
                    )
                    .foregroundStyle(TintShapeStyle().opacity((Self.activeRange ~= week) ? 1.0 : 0.5))
                }
            }
            .chartXScale(domain: Self.graphRange)
            .chartYScale(domain: 0...7)
            .chartXAxisLabel("Week")
            .chartXAxis {
                AxisMarks(values: AxisMarkValues.stride(by: 30))
            }
            .chartYAxis {
                AxisMarks(values: AxisMarkValues.stride(by: 1))
            }
        }
        .scenePadding()
    }
}

#Preview {
    HenView()
        .frame(width: HenView.previewSize.width, height: HenView.previewSize.height)
}
