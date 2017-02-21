/**
	AI Movement
	Functionality that helps the AI move.
	
	@author Sven2, Maikel
*/


// Tries to make sure the clonk stands: i.e. scales down or let's go when hangling.
public func ExecuteStand(effect fx)
{
	fx.Target->SetCommand("None");
	if (fx.Target->GetProcedure() == "SCALE")
	{
		var tx;
		if (fx.target)
			tx = fx.target->GetX() - fx.Target->GetX();
		// Scale: Either scale up if target is beyond this obstacle or let go if it's not.
		if (fx.Target->GetDir() == DIR_Left)
		{
			if (tx < -20)
				fx.Target->SetComDir(COMD_Left);
			else
				fx.Target->ObjectControlMovement(fx.Target->GetOwner(), CON_Right, 100); // let go
		}
		else
		{
			if (tx > -20)
				fx.Target->SetComDir(COMD_Right);
			else
				fx.Target->ObjectControlMovement(fx.Target->GetOwner(), CON_Left, 100); // let go
		}
	}
	else if (fx.Target->GetAction() == "Climb")
	{
		var climb_fx = GetEffect("IntClimbControl", fx.Target);
		if (climb_fx)
		{
			// For now just climb down the ladder.
			var ctrl = CON_Down;		
			EffectCall(fx.Target, climb_fx, "Control", ctrl, 0, 0, 100, false, CONS_Down);
		}	
	}
	else if (fx.Target->GetProcedure() == "HANGLE")
	{
		fx.Target->ObjectControlMovement(fx.Target->GetOwner(), CON_Down, 100);
	}
	else if (fx.Target->GetProcedure() == "FLIGHT" || fx.Target->GetAction() == "Roll")
	{
		// Don't do anything for these procedures and actions as they will end automatically.
	}
	else
	{		
		this->LogAI(fx, Format("ExecuteStand has no idea what to do for action %v and procedure %v.", fx.Target->GetAction(), fx.Target->GetProcedure()));
		// Hm. What could it be? Let's just hope it resolves itself somehow...
		fx.Target->SetComDir(COMD_Stop);
	}
	return true;
}

// Evade a threat from the given coordinates.
public func ExecuteEvade(effect fx, int threat_dx, int threat_dy)
{
	// Don't try to evade if the AI has a commander, if an AI is being commanded
	// it has more important tasks, like staying on an airship.
	if (fx.commander)
		return false;
	// Evade from threat at position delta threat_dx, threat_dy.
	if (threat_dx < 0)
		fx.Target->SetComDir(COMD_Left);
	else
		fx.Target->SetComDir(COMD_Right);
	if (threat_dy >= -5 && !Random(2))
		if (this->ExecuteJump(fx))
			return true;
	// Shield? Todo.
	return true;
}

public func ExecuteJump(effect fx)
{
	// Jump if standing on floor.
	if (fx.Target->GetProcedure() == "WALK")
	{
		if (fx.Target->~ControlJump())
			return true; // For clonks.
		return fx.Target->Jump(); // For others.
	}
	return false;
}

// Follow attack path or return to the AI's home if not yet there.
public func ExecuteIdle(effect fx)
{
	if (fx.attack_path)
	{
		var next_pt = fx.attack_path[0];
		// Check for structure to kill on path. Only if the structure is alive or of the clonk can attack structures with the current weapon.
		var alive_check;
		if (!fx.can_attack_structures && !fx.can_attack_structures_after_weapon_respawn)
		{
			alive_check = Find_OCF(OCF_Alive);
		}
		if ((fx.target = FindObject(Find_AtPoint(next_pt.X, next_pt.Y), Find_Func("IsStructure"), alive_check)))
		{
			// Do not advance on path unless target(s) destroyed.
			return true;
		}
		// Follow path
		fx.home_x = next_pt.X;
		fx.home_y = next_pt.Y;
		fx.home_dir = Random(2);
	}
	if (!Inside(fx.Target->GetX() - fx.home_x, -5, 5) || !Inside(fx.Target->GetY() - fx.home_y, -15, 15))
	{
		return fx.Target->SetCommand("MoveTo", nil, fx.home_x, fx.home_y);
	}
	else
	{
		// Next section on path or done?
		if (fx.attack_path)
		{
			if (GetLength(fx.attack_path) > 1)
			{
				fx.attack_path = fx.attack_path[1:];
				// Wait one cycle. After that, ExecuteIdle will continue the path.
			}
			else
			{
				fx.attack_path = nil;
			}
		}
		// Movement done (for now)
		fx.Target->SetCommand("None");
		fx.Target->SetComDir(COMD_Stop);
		fx.Target->SetDir(fx.home_dir);
	}
	// Nothing to do.
	return false;
}

// Turns around the AI such that it looks at its target.
public func ExecuteLookAtTarget(effect fx)
{
	// Set direction to look at target, we can assume this is instantaneous.
	if (fx.target->GetX() > fx.Target->GetX())
		fx.Target->SetDir(DIR_Right);
	else
		fx.Target->SetDir(DIR_Left);
	return true;
}