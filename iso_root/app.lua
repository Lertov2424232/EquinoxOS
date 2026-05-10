-- Глобальные константы
local SCREEN_W = 640
local SCREEN_H = 480
load_font("Inter.ttf", 24)
-- Состояние системы: "menu" (главное меню) или "transition" (анимация перехода)
local state = "menu"
local transition_start_time = 0

-- Хранилище частиц
local particles = {}

-- Инициализация частиц при запуске скрипта
math.randomseed(12345) 
for i = 1, 60 do
    table.insert(particles, {
        x = math.random(0, SCREEN_W),
        y = math.random(0, SCREEN_H),
        size = math.random(3, 10),
        speed_y = math.random(-60, -20) / 10, 
        speed_x = math.random(-15, 15) / 10,  
        phase = math.random(0, 100) / 10      
    })
end

-- Вспомогательная функция для конвертации RGB (0-255) в числовой формат (0xRRGGBB)
local function rgb_to_hex(r, g, b)
    -- math.floor здесь тоже гарантирует, что компоненты цвета будут целыми
    r = math.max(0, math.min(255, math.floor(r)))
    g = math.max(0, math.min(255, math.floor(g)))
    b = math.max(0, math.min(255, math.floor(b)))
    return r * 65536 + g * 256 + b
end
-- Главная функция обновления (вызывается 60 раз в секунду)
function on_update()
    local t = os.clock()

    ---------------------------------------------------------
    -- 1. ОТРИСОВКА ФОНА
    ---------------------------------------------------------
    local bg_r = 15 + math.sin(t * 0.5) * 10
    local bg_g = 10 + math.sin(t * 0.7) * 5
    local bg_b = 30 + math.sin(t * 0.3) * 15
    draw_rect(0, 0, SCREEN_W, SCREEN_H, rgb_to_hex(bg_r, bg_g, bg_b))

    ---------------------------------------------------------
    -- 2. ОТРИСОВКА И ОБНОВЛЕНИЕ ЧАСТИЦ
    ---------------------------------------------------------
    for i = 1, #particles do
        local p = particles[i]
        
        -- Движение (высчитывается в дробях для плавности)
        p.x = p.x + p.speed_x
        p.y = p.y + p.speed_y

        if p.y < -20 then p.y = SCREEN_H + 20 end
        if p.x < -20 then p.x = SCREEN_W + 20 end
        if p.x > SCREEN_W + 20 then p.x = -20 end

        local pr = 0
        local pg = 150 + math.sin(t * 2 + p.phase) * 100
        local pb = 200 + math.sin(t * 3 + p.phase) * 55
        
        -- ИСПРАВЛЕНИЕ: Округляем X, Y, W, H до целых пикселей
        draw_rect(math.floor(p.x), math.floor(p.y), math.floor(p.size), math.floor(p.size), rgb_to_hex(pr, pg, pb))
    end

    ---------------------------------------------------------
    -- 3. ЛОГИКА ИНТЕРФЕЙСА (МЕНЮ)
    ---------------------------------------------------------
    if state == "menu" then
        local base_w = 260
        local base_h = 60
        
        local breathe = math.sin(t * 4) 
        
        -- ИСПРАВЛЕНИЕ: Округляем размеры и позицию кнопки до целых пикселей
        local current_w = math.floor(base_w + breathe * 20)
        local current_h = math.floor(base_h + breathe * 5)
        
        local btn_x = math.floor((SCREEN_W - current_w) / 2)
        local btn_y = math.floor((SCREEN_H - current_h) / 2)

        if button("START SYSTEM", btn_x, btn_y, current_w, current_h) then
            state = "transition"
            transition_start_time = t
        end
        draw_text("Beautiful Vector Text", 100, 100, 0xFFFFFF)

    ---------------------------------------------------------
    -- 4. АНИМАЦИЯ ПЕРЕХОДА
    ---------------------------------------------------------
    elseif state == "transition" then
        local anim_duration = 1.5 
        local progress = (t - transition_start_time) / anim_duration
        if progress > 1 then progress = 1 end

        local grid_size = 40
        local cols = math.ceil(SCREEN_W / grid_size)
        local rows = math.ceil(SCREEN_H / grid_size)

        for row = 0, rows - 1 do
            for col = 0, cols - 1 do
                local dx = col - (cols / 2)
                local dy = row - (rows / 2)
                local dist = math.sqrt(dx * dx + dy * dy)
                local max_dist = math.sqrt((cols / 2)^2 + (rows / 2)^2)

                local delay = (dist / max_dist) * 0.5
                local sq_progress = (progress - delay) / 0.5 
                
                if sq_progress < 0 then sq_progress = 0 end
                if sq_progress > 1 then sq_progress = 1 end

                sq_progress = 1 - (1 - sq_progress)^3

                -- ИСПРАВЛЕНИЕ: Округляем координаты и размер каждого квадрата в анимации
                local size = math.floor(grid_size * sq_progress)
                local cx = col * grid_size + grid_size / 2
                local cy = row * grid_size + grid_size / 2

                if size > 0 then
                    local rx = math.floor(cx - size / 2)
                    local ry = math.floor(cy - size / 2)
                    draw_rect(rx, ry, size, size, 0x00FFCC)
                end
            end
        end

        if progress == 1 then
            draw_rect(0, 0, SCREEN_W, SCREEN_H, 0x00FFCC)
        end
    end
end