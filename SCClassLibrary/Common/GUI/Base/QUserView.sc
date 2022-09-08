UserView : View {
	var <drawFunc, <drawingEnabled=true, <animate=false;
	// MULTI-TOUCH
	var <touchBeginAction, <touchUpdateAction, <touchEndAction, <touchCancelAction;

	*qtClass { ^'QcCustomPainted' }

	*new { arg parent, bounds;
		var me = super.new(parent, bounds ?? {this.sizeHint} );
		me.canFocus = true;
		^me;
	}

	*sizeHint {
		^Point(150,150);
	}

	drawingEnabled_ { arg boolean;
		// Allow setting the property apart from the instance variable
		// to optimize when drawFunc is nil. See drawFunc_ implementation.
		drawingEnabled = boolean;
		this.setProperty( \drawingEnabled, boolean );
	}

	clearOnRefresh { ^this.getProperty( \clearOnRefresh ); }
	clearOnRefresh_ { arg boolean; this.setProperty( \clearOnRefresh, boolean ); }

	clearDrawing { this.invokeMethod( \clear ); }

	drawFunc_ { arg aFunction;
		this.setProperty( \drawingEnabled, aFunction.notNil );
		drawFunc = aFunction;
	}

	draw {
		// NOTE: it is only allowed to call this while a QPaintEvent is being
		// processed by this QWidget, or an error will be thrown.
		drawFunc.value(this);
	}

	animate_ { arg bool; this.invokeMethod( \animate, bool ); animate = bool; }

	frameRate_ { arg fps; this.setProperty( \frameRate, fps.asFloat ); }
	frameRate { ^this.getProperty( \frameRate ); }

	frame { ^this.getProperty( \frameCount ); }

	// override View's action_ to not connect to 'action()' signal
	action_ { arg func;
		action = func;
	}

	doDrawFunc { drawFunc.value(this) }

	// MULTI-TOUCH

	touchBeginAction_ { arg aFunction;
		touchBeginAction = aFunction;
		this.setEventHandler(QObject.touchBeginAction, \touchBegin, aFunction.notNil);
	}
	touchBegin { |...args|
		var touchPointsInfo = args.clump(4);
		touchBeginAction.value(this, TouchPoint.fromEventArgs(args))
	}

	touchUpdateAction_ { arg aFunction;
		touchUpdateAction = aFunction;
		this.setEventHandler(QObject.touchUpdateAction, \touchUpdate, aFunction.notNil);
		// touchBegin needs to be enabled for a widget to receive touchUpdates
		// enable it automatically if the user sets only touchUpdateAction
		if (touchBeginAction.isNil && touchUpdateAction.notNil) {
			this.touchBeginAction_{}
		}
	}
	touchUpdate { |...args|
		touchUpdateAction.value(this, TouchPoint.fromEventArgs(args))
	}

	touchEndAction_ { arg aFunction;
		touchEndAction = aFunction;
		this.setEventHandler(QObject.touchEndAction, \touchEnd, aFunction.notNil);
	}
	touchEnd { |...args|
		touchEndAction.value(this, TouchPoint.fromEventArgs(args)) 
	}


}

TouchPoint {
	var <id, <x, <y, <state;
	classvar states = #[\pressed, \moved, \stationary, \released];

	*new { |id, x, y, stateInt|
		var state = states[stateInt];
		^super.newCopyArgs(id, x, y, state)
	}

	*fromEventArgs { |...args|
		^args.clump(4).collect(TouchPoint(*_))
	}

	asPoint { ^Point(x, y) }
}
