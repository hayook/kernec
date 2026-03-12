
local kernel = require("examples.fifo-prio")

onEvent("create", kernel.create_cb)
onEvent("init", kernel.init_cb)
onEvent("exit", kernel.exit_cb)
