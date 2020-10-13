BelaScope {

	classvar <serverScopes;
	var <server, <maxChannels, <bus, <node;
	var bootFunction, treeFunction;

	*scope { |channelOffset, signals, server|
		var scope = serverScopes[server ? Server.default];
		if(scope.notNil) {
				^scope.scope(channelOffset, signals);
		} {
				// TODO: BelaScope needs to be initialized for server
		};
	}

	*initClass {
		serverScopes = IdentityDictionary[];
	}

	*new { |server, maxChannels=8|
		server = server ? Server.default;
		maxChannels = if(maxChannels > 0) { maxChannels } { 8 };

		if(serverScopes[server].isNil) {
				serverScopes[server] = super.newCopyArgs(server, maxChannels).init;
		}{
				// scope already exists, check maxChannels
		};
		^serverScopes[server];
	}

	scope { |channelOffset, signals|

		var ugens = signals.asArray.collect{ |item|
				switch(item.rate)
					{ \audio }{ item } // pass
					{\control}{ K2A.ar(item) } // convert kr to ar
					{\scalar}{
						// convert numbers to ar UGens
						if(item.isNumber) { DC.ar(item) } { nil }
					}
					{ nil }
		};

		if(channelOffset + signals.size > this.maxChannels) {
				"BelaScope: can't scope this signal, max number of channels (%) exceeded.\nSignal: %"
				.format(this.maxChannels).warn;
				^signals;
		};

		if( ugens.every(_.isUGen) ){
				^Out.ar(this.bus.index + channelOffset, ugens);
		} {
				"BelaScope: can't scope this signal, because not all of its elements are UGens.\nSignal: %"
				.format(signals).warn;
				^signals
		}
	}

	init {
		bootFunction = { this.prReserveScopeBus };
		treeFunction = { this.prStartScope };

		ServerBoot.add(bootFunction, this.server);
		ServerTree.add(treeFunction, this.server);
		if(this.server.serverRunning){
				bootFunction.value();
				treeFunction.value();
		}
	}

	prReserveScopeBus {
		// TODO: check if bus is already reserved, or if maxChannels mismatch
		bus = Bus.audio(server, maxChannels);
	}

	prStartScope {
		// TODO: check if node is already in place and running
		node = { BelaScopeUGen.ar(this.maxChannels, this.bus); Silent.ar }.play(this.server, addAction: \addAfter);
	}
}

+ UGen {
	belaScope { |scopeChannel, server|
		^BelaScope.scope(scopeChannel, this, server)
	}
}

+ Array {
	belaScope { |scopeChannel, server|
		^BelaScope.scope(scopeChannel, this, server)
	}
}
