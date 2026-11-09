-- drive until the bumper feels something
while not bump() do
  motors(0.1, 0.1)
end
say("Ouch!")
