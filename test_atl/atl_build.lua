local src_files = atl.list.source_files()
for i, f in pairs(src_files) do
	print(f)
end

--os.execute("bat " .. src_files[5])
--os.execute("bat " .. src_files[6])

project_id = atl.project.create {
	name = "test",
	build_type = "release"
}

project_id = atl.project.create {
	name = "test2",
	source_files = src_files,
}

local p = atl.project.get("test")
print(p.build_type)
print(p.id)

local p2 = atl.project.get("test2")
print(p2.build_type)
print(p2.id)
print(p2.root_dir)
print(p2.source_files)
print(p2.source_files[2])

print("LuaC is awsome bro")
