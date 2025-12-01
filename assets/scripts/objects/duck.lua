-- Duck Demo Script
-- Shows a glTF sample model with simple animation

function define(obj)
    obj.name = "Duck (glTF)"
    obj.selection_radius = 3.0
    obj.move_speed = 5.0
    obj.turn_speed = 90.0
    obj.can_move = true
    obj.scale = vec3.new(0.01, 0.01, 0.01)  -- Duck model is large, scale down

    -- Main body (single part from glTF)
    local body = Part.new()
    body.name = "body"
    body.model_path = "assets/models/samples/duck.glb"
    body.offset = vec3.new(0, 0, 0)
    body.scale = vec3.new(1, 1, 1)
    body.can_rotate = false
    body.parent_index = -1
    obj:add_part(body)
end

function init(obj, state)
    state.bob_time = 0.0
    state.bob_height = 0.0
    print("Duck initialized: " .. obj.name)
end

function update(obj, dt, state)
    -- Initialize state if needed
    if state.bob_time == nil then
        state.bob_time = 0.0
        state.bob_height = 0.0
    end

    -- Simple bobbing animation
    state.bob_time = state.bob_time + dt * 2.0
    state.bob_height = math.sin(state.bob_time) * 0.1

    -- Apply bob to Y position (handled by game)
    -- obj.position.y = state.bob_height
end

function on_select(obj, state)
    print("Duck selected - glTF sample model")
end

function on_deselect(obj, state)
    print("Duck deselected")
end

function on_move(obj, target, state)
    print("Duck waddling to: " .. target.x .. ", " .. target.z)
end

