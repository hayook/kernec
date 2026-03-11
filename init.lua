
local ready_queue = {}

function scheduler()
	local p = table.remove(ready_queue, 1)
	if p == nil then
		return
	end
	setCurrent(p)
	-- start_process(p)
	print("[ OS ] CPU will be assigned to process " .. tostring(p.pid) .. ".")
end

onEvent("create", function(p)
	table.insert(ready_queue, p)
	print("[Lua Scheduler] Process " .. tostring(p.pid) .. " added to the ready_queue.")
end)

onEvent("init", function()
	scheduler()
end)

onEvent("exit", function()
	scheduler()
end)
