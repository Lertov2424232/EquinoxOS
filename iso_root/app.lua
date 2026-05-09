-- Тестируем математику и строки
local x = 0.5
local y = math.sin(x)
print("Lua Math Test: sin(0.5) = " .. tostring(y))

-- Тестируем выделение памяти (создаем кучу таблиц)
print("Testing GC...")
local t = {}
for i=1, 1000 do
    t[i] = "String number " .. i
end
t = nil
collectgarbage()
print("GC looks stable!")

-- Тестируем EID (если ты прокинул функции)
draw_rect(50, 50, 200, 150, 0x334455)
if button("Equinox Lua Кнопка", 70, 80, 160, 30) then
    draw_rect(0, 0, 400, 300, 0x00FF00) -- Заливаем всё зеленым при клике
end