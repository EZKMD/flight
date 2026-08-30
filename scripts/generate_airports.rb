#!/usr/bin/env ruby
# frozen_string_literal: true

require "csv"

abort "usage: generate_airports.rb INPUT.csv OUTPUT.inc" unless ARGV.length == 2

records = CSV.read(ARGV[0], headers: true).each_with_object([]) do |row, selected|
  next unless row["type"] == "large_airport"
  next unless row["scheduled_service"] == "yes"
  next if row["iata_code"].to_s.empty? || row["icao_code"].to_s.empty?

  selected << {
    iata: row["iata_code"],
    icao: row["icao_code"],
    name: row["name"],
    latitude: row["latitude_deg"],
    longitude: row["longitude_deg"]
  }
end.sort_by { |record| record[:iata] }

File.open(ARGV[1], "w") do |file|
  file.puts "/* Generated from OurAirports airports.csv; see data/README.md. */"
  records.each do |record|
    escaped_name = record[:name].gsub("\\", "\\\\").gsub('"', '\\"')
    file.puts %(    { "#{record[:iata]}", "#{record[:icao]}", "#{escaped_name}", ) +
              %(#{record[:latitude]}, #{record[:longitude]}, "" },)
  end
end
