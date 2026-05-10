-- desktop.lua (Equinox Desktop Environment)

local screen_w = 640
local screen_h = 480
local taskbar_h = 32
local menu_open = false

function on_update()
    -- 1. Обои (Глубокий фиолетово-синий градиент/фон)
    draw_rect(0, 0, screen_w, screen_h, 0x1A0F2E) 

    -- Абстрактные геометрические элементы на фоне
    draw_rect(100, 150, 120, 120, 0x24153D)
    draw_rect(450, 250, 80, 80, 0x2D1A4C)

    -- 2. Иконки на рабочем столе
    if button("Snake", 20, 30, 80, 30) then
        sys_exec("snake.elf")
    end
    if button("Doom", 20, 70, 80, 30) then
        sys_exec("doom.elf")
    end
    if button("BMP View", 20, 110, 80, 30) then
        sys_exec("bmpview.elf")
    end

    -- 3. Панель задач (KDE Style, внизу)
    local tb_y = screen_h - taskbar_h
    draw_rect(0, tb_y, screen_w, taskbar_h, 0x0D0717) -- Очень темный фиолетовый
    draw_rect(0, tb_y, screen_w, 1, 0x3D246C) -- Тонкая фиолетовая полоса сверху

    -- 4. Кнопка меню "Пуск" (Equinox Logo)
    if button(" EQUINOX ", 5, tb_y + 4, 80, 24) then
        menu_open = not menu_open
    end

    -- 5. Меню Пуск (Mac Style - плавающее)
    if menu_open then
        local menu_w = 200
        local menu_h = 220
        local menu_x = 5
        local menu_y = tb_y - menu_h - 5
        
        -- Фон меню
        draw_rect(menu_x, menu_y, menu_w, menu_h, 0x180D2A)
        -- Обводка меню
        draw_rect(menu_x, menu_y, menu_w, 1, 0x4D347C)
        draw_rect(menu_x + menu_w, menu_y, 1, menu_h, 0x4D347C)
        draw_rect(menu_x, menu_y, 1, menu_h, 0x4D347C)

        draw_text(menu_x + 20, menu_y + 10, "System Menu", 0x9D84CC)
        draw_rect(menu_x + 10, menu_y + 25, menu_w - 20, 1, 0x3D246C)

        -- Элементы меню
        if button("System Monitor", menu_x + 10, menu_y + 40, menu_w - 20, 30) then
            -- Пока нет системного монитора на Луа, но мы его напишем!
            menu_open = false
        end
        if button("Notepad", menu_x + 10, menu_y + 75, menu_w - 20, 30) then
            sys_exec("notepad.elf")
            menu_open = false
        end
        if button("Reboot", menu_x + 10, menu_y + menu_h - 40, menu_w - 20, 30) then
            -- sys_exec("reboot")
        end
    end

    -- 6. Часы / Статус
    draw_text(screen_w - 70, tb_y + 12, "SYS: OK", 0xAAAAAA)
end