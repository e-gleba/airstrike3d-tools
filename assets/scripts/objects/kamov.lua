-- Kamov Ka-50 Helicopter Script
-- Defines a helicopter with coaxial rotors (counter-rotating)

function define(obj)
    obj.name = "Ka-50 Kamov"
    obj.selection_radius = 6.0
    obj.move_speed = 25.0
    obj.turn_speed = 90.0
    obj.can_move = true

    -- Main body
    local body = Part.new()
    body.name = "body"
    body.model_path = "assets/models/helics/kamov/kamov.obj"
    body.offset = vec3.new(0.00, 0.00, 0.00)
    body.scale = vec3.new(0.1, 0.1, 0.1)
    body.can_rotate = false
    body.parent_index = -1
    obj:add_part(body)

    -- Main rotor (top, spins counter-clockwise when viewed from above)
    local rotor_main = Part.new()
    rotor_main.name = "rotor_main"
    rotor_main.model_path = "assets/models/helics/vints/vint_c.obj"
    rotor_main.offset = vec3.new(0.60, 2.30, 0.20)
    rotor_main.scale = vec3.new(0.300, 0.300, 0.300)
    rotor_main.rotation_axis = vec3.new(0, 1, 0) -- Y axis
    rotor_main.rotation_speed = 720.0 -- Fast spin
    rotor_main.can_rotate = true
    rotor_main.continuous = true -- Always spinning
    rotor_main.parent_index = 0
    obj:add_part(rotor_main)

    -- Counter rotor (below main, spins clockwise - negative speed)
    local rotor_counter = Part.new()
    rotor_counter.name = "rotor_counter"
    rotor_counter.model_path = "assets/models/helics/vints/vint_c.obj"
    rotor_counter.offset = vec3.new(0.60, 1.85, 0.20)
    rotor_counter.scale = vec3.new(0.300, 0.300, 0.300)
    rotor_counter.rotation_axis = vec3.new(0, 1, 0)
    rotor_counter.rotation_speed = -720.0 -- Opposite direction
    rotor_counter.can_rotate = true
    rotor_counter.continuous = true
    rotor_counter.parent_index = 0
    obj:add_part(rotor_counter)
end

function init(obj, state)
    state.hover_offset = 0.0
    state.hover_time = 0.0
    state.base_height = 5.0 -- Flight altitude
    print("Kamov initialized: " .. obj.name)
end

function update(obj, dt, state)
    -- Initialize state if needed
    if state.hover_time == nil then
        state.hover_time = 0.0
        state.hover_offset = 0.0
        state.base_height = 5.0
    end

    -- Hovering bob effect
    state.hover_time = state.hover_time + dt
    state.hover_offset = math.sin(state.hover_time * 2.0) * 0.3

    -- Apply hover to Y position (helicopter floats)
    -- The game will handle the base height, we just add the bob
    -- obj.position.y = state.base_height + state.hover_offset
end

function on_select(obj, state)
    print("Kamov selected - coaxial rotor helicopter")
end

function on_deselect(obj, state)
    print("Kamov deselected")
end

function on_move(obj, target, state)
    -- Helicopters move directly to target
    print("Kamov flying to: " .. target.x .. ", " .. target.z)
end