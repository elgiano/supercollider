+ String {
	runInTerminal {
		format("start \"SuperCollider runInTerminal\" cmd.exe /k %", this.quote).unixCmd;
	}

	openOS { |action|
		// start "title" "command"
		["start", "SuperCollider", this].unixCmd(action)
	}
}
