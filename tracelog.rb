def tarai( x, y, z )
  if x <= y
  then y
  else tarai(tarai(x-1, y, z),
             tarai(y-1, z, x),
             tarai(z-1, x, y))
  end
end

tarai(8, 4, 0)

require 'json'

class TraceLog
  def self.to_json
    pid = Process.pid

    events = to_a.map {|name, cat, ph, ts|
      {
        name: name,
        cat: cat,
        ph: ph,
        pid: pid,
        tid: 1234, # FIXME
        ts: ts / 1000
      }
    }
    # events.each {|e| p e }

    {
      traceEvents: events,
      otherData: {
        version: "ruby #{RUBY_VERSION}" 
      }
    }.to_json    
  end
end

open("trace.json", "w") {|f| f.write TraceLog.to_json }
STDERR.puts "DONE!"
