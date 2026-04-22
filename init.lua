
local programs = require("programs")
local kernel = require("examples.fifo-prio")

job = { programs.p1, programs.p2 }

onEvent("create", kernel.create_cb)
onEvent("init", kernel.init_cb)
onEvent("exit", kernel.exit_cb)
