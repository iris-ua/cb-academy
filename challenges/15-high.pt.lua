while true do
  local a = beacon()
  if a == nil then
    say("Não vejo o farol")
  end
  motors(0.1, 0.1)
end
