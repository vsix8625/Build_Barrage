local info = atl.os.info()
local endian = info.little_endian and "little_endian" or "big_endian"

print("Name: " .. info.name)
print("Version: " .. info.version)
print("Machine: " .. info.machine)
print("Cores: " .. info.cores)
print("Endian: " .. endian)
print("Hostaname: " .. info.hostname)
