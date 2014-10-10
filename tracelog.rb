def tarai( x, y, z )
  if x <= y
  then y
  else tarai(tarai(x-1, y, z),
             tarai(y-1, z, x),
             tarai(z-1, x, y))
  end
end

tarai(3, 2, 0)

# require 'json'

class TraceLog
  def self.to_json
    events = to_a.map {|name, cat, ph|
      {
        name: name,
        cat: cat,
        ph: ph,
        pid: 123,
        tid: 456,
        ts: Time.now
      }
    }

    {
      traceEvents: events,
      otherData: {
        version: "ruby #{RUBY_VERSION}" 
      }
    }#.to_json    
  end
end

p TraceLog.to_json
