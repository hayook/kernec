
onEvent("create", function(p)
	print("[LUA API] create event pid=" .. tostring(p.pid))
end)

onEvent("init", function()
	print("[LUA API] init event")
end)

onEvent("exit", function()
	print("[LUA API] exit event")
end)
