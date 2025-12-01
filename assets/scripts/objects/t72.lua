-- T72 Tank Script
-- Defines a tank with rotating turret and gun

function define(obj)
    obj.name = "T-72 Tank"
    obj.selection_radius = 4.0
    obj.move_speed = 8.0
    obj.turn_speed = 45.0
    obj.can_move = true

    -- Base (hull)
    local base = Part.new()
    base.name = "base"
    base.model_path = "assets/models/tanks/t72/t72_base.obj"
    base.offset = vec3.new(0.00, 0.00, 0.00)
    base.scale = vec3.new(0.100, 0.100, 0.100)
    base.can_rotate = false
    base.parent_index = -1
    obj:add_part(base)

    -- Turret (rotates on Y axis)
    local turret = Part.new()
    turret.name = "turret"
    turret.model_path = "assets/models/tanks/t72/t72_turret.obj"
    turret.offset = vec3.new(0.00, 2.00, 0.00)
    turret.scale = vec3.new(0.100, 0.100, 0.100)
    turret.rotation_axis = vec3.new(0, 1, 0) -- Y axis
    turret.rotation_speed = 30.0 -- Degrees per second
    turret.min_angle = -180.0
    turret.max_angle = 180.0
    turret.can_rotate = true
    turret.continuous = false
    turret.parent_index = 0 -- Parent is base
    obj:add_part(turret)

    -- Main gun (rotates on X axis for elevation)
    local gun = Part.new()
    gun.name = "gun"
    gun.model_path = "assets/models/tanks/t72/t72_gun.obj"
    gun.offset = vec3.new(0.00, 2.45, 0.00)
    gun.scale = vec3.new(0.100, 0.100, 0.100)
    gun.rotation_axis = vec3.new(1, 0, 0) -- X axis for elevation
    gun.rotation_speed = 15.0
    gun.min_angle = -5.0 -- Depression
    gun.max_angle = 20.0 -- Elevation
    gun.can_rotate = true
    gun.continuous = false
    gun.parent_index = 1 -- Parent is turret
    obj:add_part(gun)
end

function init(obj, state)
    state.turret_target = 0.0
    state.gun_target = 0.0
    print("T-72 initialized: " .. obj.name)
end

function update(obj, dt, state)
    -- Get turret and gun parts
    local turret = obj:get_part("turret")
    local gun = obj:get_part("gun")

    if turret and obj.has_target then
        -- Calculate angle to target
        local dx = obj.target_pos.x - obj.position.x
        local dz = obj.target_pos.z - obj.position.z
        local target_angle = math.deg(math.atan(dx, dz))

        -- Adjust for object rotation
        target_angle = target_angle - obj.rotation.y
        target_angle = normalize_angle(target_angle)

        turret.target_angle = target_angle
    end

    -- Slight gun bob when moving
    if gun and obj.current_speed > 0.1 then
        local bob = math.sin(os.clock() * 5) * 0.5
        gun.target_angle = clamp(bob, gun.min_angle, gun.max_angle)
    end
end

function on_select(obj, state)
    print("T-72 selected")
end

function on_deselect(obj, state)
    print("T-72 deselected")
end

function on_move(obj, target, state)
    print("T-72 moving to: " .. target.x .. ", " .. target.z)
end
