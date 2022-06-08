#!/usr/bin/env ruby

filename = ARGV[0]
if filename.nil?
    puts "Usage: mapper.rb <logfile>"
    exit(1)
end
require 'ruby2d'

RECT_LENGTH = 1024
RECT_HEIGHT = 8
PAGE_SIZE = 4096
LEFT_EDGE = 10
TOP_EDGE = 10
INTER_RECT_SPACE = 4
XSCALE = (PAGE_SIZE * 1.0 / RECT_LENGTH)

BLOCK_COLORS = ["#546223", "#FFFFFF", "#522D80", "#CBC4BC", "#EFDBB2", "#005EB8", "#F56600", "#C8C9C7"]

def createRect(page, offset, size, activepages)
    pageidx = activepages.keys.index(page)

    scaledoffset = offset / XSCALE
    scaledsize = size / XSCALE
    Rectangle.new(
        x: LEFT_EDGE + scaledoffset,
        y: TOP_EDGE + ((RECT_HEIGHT+INTER_RECT_SPACE) * pageidx),
        width: scaledsize,
        height: RECT_HEIGHT,
        color: BLOCK_COLORS.sample()
    )
end

def process_operation(op, activeblocks, activepages)
    puts "OP: #{op}"
    if (op[:action] == 'M')
        puts "Malloc"
        if activeblocks.has_key? op[:addr]
            #error, double malloc!!!!
            puts "ERROR: Malloc returned address again?!?! #{op}"
            exit(1)
        end
        activeblocks[op[:addr]] = op
        bytes = op[:size]
        current_page = op[:page]
        current_offset = op[:offset]
        while (bytes > 0)
            activepages[current_page] ||= []
            chunksize = [bytes, PAGE_SIZE - current_offset].min

            newchunk = {:addr => op[:addr], :offset => current_offset, :size => chunksize}
            newchunk[:rect] = createRect(current_page, current_offset, chunksize, activepages)

            activepages[current_page].push newchunk

            bytes -= chunksize
            current_offset = 0
            current_page += 1
        end

    elsif (op[:action] = 'F')
        puts "Free (#{op[:addr]})"
        freedblock = activeblocks.delete op[:addr]
        if !freedblock
            #error, not malloc-ed
            puts "ERROR: Can't find block! #{op}\nIgnoring, for now."
            return
        end

        current_page = freedblock[:page]
        bytes = freedblock[:size]
        # go through and remove any matching blocks
        while (bytes > 0)
            activepages[current_page].delete_if { |chunk|
                if (chunk[:addr] == freedblock[:addr])
                    bytes -= chunk[:size]
                    chunk[:rect].remove()
                    true
                else
                    false
                end
            }

            current_page += 1
        end
    else
        puts "ERROR: unknown action\n#{op}"
        exit(1)
    end
end

#first read in all of the data
lines = File.readlines(ARGV[0])
ops = lines.map { |line|
    toks = line.chomp.split(",")
    result = {:action => toks[0], :addr => toks[1].to_i}
    result[:page] = result[:addr] >> 12
    result[:offset] = result[:addr] & 0xFFF
    if (result[:action] == "M")
        result[:size] = toks[2].to_i
    end

    result
}
activeblocks = {}
activepages = {}





set title: "MallocMapper - " + filename
set width: 1200
set height: 700

tick = 0
current_op = 0

update do
    tick += 1
    if (tick % 1 == 0)
        process_operation(ops[current_op], activeblocks, activepages)
        if (current_op < ops.size)
            current_op += 1
        end
    end
end

show