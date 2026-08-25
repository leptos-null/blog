//
//  PlayAreaView.swift
//  ChartRender
//
//  Created by Leptos on 8/23/26.
//

import SwiftUI
import Charts

struct WeeklyRevenue: Identifiable {
    var id: Int { week }

    var week: Int
    var revenue: Int

    static func revenueForWeek(_ x: Int) -> Int {
        precondition((1...52).contains(x))

        // proxy for seasonal temperature:
        //   -1 at week 1 (deep winter), +1 at week 26 (peak summer)
        let raw: Double = -cos(2 * Double.pi * Double(x - 1) / 52)
        // cube root flattens the plateau near the winter/summer extremes and
        // steepens the spring/fall transition, so adjacent summer weeks read as similar
        let seasonal: Double = cbrt(raw)

        let baseline: Double = 1500 + 450 * seasonal
        let variance: Double = 400 + 150 * seasonal

        let modeled: Double = baseline + Double.random(in: -variance...variance)
        let rounded: Int = Int(modeled.rounded())
        return rounded - (rounded % 5)
    }
}

struct PlayAreaView: View {
    static let previewSize: CGSize = .init(width: 720, height: 450)

    var isRender: Bool = false

    @Environment(\.self) private var fullEnvironment

    // sum: 79,605
    let revenues: [WeeklyRevenue] = [
        .init(week: 1, revenue: 985),
        .init(week: 2, revenue: 1255),
        .init(week: 3, revenue: 1195),
        .init(week: 4, revenue: 1020),
        .init(week: 5, revenue: 995),
        .init(week: 6, revenue: 1205),
        .init(week: 7, revenue: 965),
        .init(week: 8, revenue: 1155),
        .init(week: 9, revenue: 1230),
        .init(week: 10, revenue: 1185),
        .init(week: 11, revenue: 1300),
        .init(week: 12, revenue: 985),
        .init(week: 13, revenue: 1250),
        .init(week: 14, revenue: 1755),
        .init(week: 15, revenue: 2115),
        .init(week: 16, revenue: 1895),
        .init(week: 17, revenue: 1670),
        .init(week: 18, revenue: 1405),
        .init(week: 19, revenue: 1590),
        .init(week: 20, revenue: 1480),
        .init(week: 21, revenue: 1590),
        .init(week: 22, revenue: 1760),
        .init(week: 23, revenue: 2465),
        .init(week: 24, revenue: 1530),
        .init(week: 25, revenue: 1675),
        .init(week: 26, revenue: 2320),
        .init(week: 27, revenue: 2330),
        .init(week: 28, revenue: 2350),
        .init(week: 29, revenue: 1830),
        .init(week: 30, revenue: 1975),
        .init(week: 31, revenue: 1720),
        .init(week: 32, revenue: 2260),
        .init(week: 33, revenue: 1860),
        .init(week: 34, revenue: 1875),
        .init(week: 35, revenue: 1350),
        .init(week: 36, revenue: 2135),
        .init(week: 37, revenue: 2245),
        .init(week: 38, revenue: 2145),
        .init(week: 39, revenue: 2065),
        .init(week: 40, revenue: 1425),
        .init(week: 41, revenue: 1595),
        .init(week: 42, revenue: 1350),
        .init(week: 43, revenue: 1250),
        .init(week: 44, revenue: 1295),
        .init(week: 45, revenue: 980),
        .init(week: 46, revenue: 1160),
        .init(week: 47, revenue: 1235),
        .init(week: 48, revenue: 1275),
        .init(week: 49, revenue: 835),
        .init(week: 50, revenue: 1040),
        .init(week: 51, revenue: 995),
        .init(week: 52, revenue: 1055),
    ]

    private static func render(in environment: EnvironmentValues) {
        ViewExport.render(
            content: PlayAreaView(isRender: true),
            size: Self.previewSize,
            environment: environment,
            baseName: "play_area"
        )
    }

    private static func generateRevenues() {
        let generate: [WeeklyRevenue] = (1...52).map { week in
            return .init(week: week, revenue: WeeklyRevenue.revenueForWeek(week))
        }

        let total: Int = generate.map(\.revenue).reduce(into: 0, +=)
        print("// sum: \(total)")
        for revenue in generate {
            print(".init(week: \(revenue.week), revenue: \(revenue.revenue)),")
        }
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
                }
                .padding(.horizontal)
            }

            Chart {
                ForEach(self.revenues) { revenue in
                    BarMark(
                        x: .value("Week", revenue.week),
                        yStart: .value("Floor", 0),
                        yEnd: .value("Dollars", revenue.revenue),
                        width: MarkDimension.fixed(7)
                    )
                    .foregroundStyle(TintShapeStyle())
                }
            }
            .chartXScale(domain: 1...54)
            .chartXAxisLabel("Week")
            .chartXAxis {
                AxisMarks(values: AxisMarkValues.stride(by: 8))
            }
        }
        .scenePadding()
    }
}

#Preview {
    PlayAreaView()
        .frame(width: PlayAreaView.previewSize.width, height: PlayAreaView.previewSize.height)
}
