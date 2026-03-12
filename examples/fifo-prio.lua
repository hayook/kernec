
local kernel = {}

-- Simulate processes priority based on pids only to showcase uservalues
function estimate_priority(p)
	if p.pid % 2 == 0 then
		return 0
	end
	return 1
end

local ready_queue = {}

function scheduler()
	-- sort the ready_queue based on the priority
	table.sort(ready_queue, function(a, b)
		return a.prio > b.prio
	end)
	local p = table.remove(ready_queue, 1)
	if p == nil then
		return
	end
	setCurrent(p)
	-- start_process(p)
	print("[ OS ] CPU will be assigned to process " .. tostring(p.pid) .. ".")
end

kernel.create_cb = function(p)
	p.prio = estimate_priority(p)
	table.insert(ready_queue, p)
	print("[Lua Scheduler] Process " .. tostring(p.pid) .. " added to the ready_queue.")
end

kernel.init_cb = function()
	scheduler()
end

kernel.exit_cb = function()
	scheduler()
end

return kernel
