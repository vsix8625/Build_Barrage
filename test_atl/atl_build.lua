local start = os.clock()

local info = atl.os.info()
local endian = info.little_endian and "little_endian" or "big_endian"

print("Name: " .. info.name)
print("Version: " .. info.version)
print("Machine: " .. info.machine)
print("Cores: " .. info.cores)
print("Endian: " .. endian)
print("Hostname: " .. info.hostname)
print("Cores: " .. info.cores)
print("Cache Line size: " .. info.cache_line_size)

local env = atl.env.all()
print("Home: " .. env.HOME)
print("PATH: " .. env.PATH)
print("Desktop: " .. env.XDG_CURRENT_DESKTOP)

local ends = os.clock()


print("Time: " .. ends - start .. "seconds")
