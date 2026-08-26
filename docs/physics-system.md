# The physics system of Waveforge

Waveforge uses a custom physics system to simulate the movement and interactions of entities within the game world. Different from most physics engines (like Box2D), Waveforge's physics system is pixel-simulation-based, meaning that it operates on a per-pixel basis rather than using continuous mathematics. The system is actually a cellular automaton with many rules that govern how entities move and interact with the environment.

The production pixel world is authoritative on the GPU through `wgpu-native`.
Each tick applies batched CPU edits, updates active 32x32 chunks, runs thermal
and material transitions, resolves deterministic movement proposals, performs
local liquid equalization, propagates electricity, and produces rendering and
query output in one command encoder. Stateless hashes based on the tick, cell,
pass, and direction make parallel choices reproducible.

Gameplay entities remain on the CPU. The duck requests a padded region around
its current and predicted position, while structures request their sensor
regions. They consume immutable results from the previous completed GPU tick
and submit changes for the next tick. Normal simulation never reads back the
full packed world; full state readback is reserved for explicit serialization.
RGBA output uses a three-buffer asynchronous readback ring for SFML upload.

The former CPU simulator remains available when WebGPU is disabled for
reference benchmarks and developer tests. Production builds enable WebGPU by
default, do not allocate the CPU `PixelElement` proxy array, and report device
loss instead of silently switching simulation rules.

Furthermore, some pixels of a particular pattern can be grouped together to form _structures_. A structure can have special behaviors that differ from the individual pixels composing it.

## Pixel Classes

Pixels in Waveforge are categorized into 4 different classes based on their physical properties:

- **Gas Pixels**: These pixels represent gases. They rise and spread out in the environment.

- **Fluid Pixels**: These pixels represent liquids. They flow and fill spaces in the environment.

- **Solid Pixels**: These pixels represent solid materials. Some solids can be moved or destroyed (e.g., sand), while others are immovable (e.g., stone).

- **Particle Pixels**: These pixels are used internally for special effects.

Note: The "Air" pixel, although classified as a gas pixel, does not behave like other gas pixels. It serves as the default state of pixels in the game world.

## Notable simulation systems

### Thermodynamics

Waveforge simulates basic thermodynamics, allowing pixels to have temperature properties. Heat can be transferred between pixels, and certain pixels can change state based on their temperature (e.g., water evaporating into steam).

### Fire propagation

Some pixels will be ignited when exposed to high temperatures. Fire can produce massive heat, which can spread to nearby flammable pixels, causing them to catch fire as well.

### Explosions

Note: this system is not yet implemented.

Explosions can occur when certain pixels reach critical conditions. Explosions will create shockwaves that can move or destroy nearby pixels, depending on their properties.

### Electrical signaling

Some pixels can conduct electricity. Electrical signals can be used to trigger certain behaviors in pixels. Some _structures_ respond to electrical signals, allowing for the creation of simple circuits within the game world.

### Laser beams

Some structures can emit laser beams that can interact with other pixels / structures. Laser beams goes in straight lines until they hit an solid pixel or smoke. Lasers can be used to trigger mechanisms and transfering signals over long distances. Laser beams can also heat up solid pixels that they hit.

## Basic Pixel Elements

### Air

Air is the default state of pixels in the game world. It has no special properties or behaviors. The amount of air pixels is not guaranteed to be conserved during simulation steps.

### Steam

Steam is a type of gas pixel. Steam will condense into water when it cools down sufficiently. Water will evaporate into steam when heated enough.

### Smoke

Smoke is a type of gas pixel. Smoke rises and spreads out in the environment. It can obscure vision and affect laser beams. Smoke will automatically disappear after some time.

### Water

Water is a type of fluid pixel. Water flows and fills spaces in the environment. It will evaporate into steam when heated enough.

### Oil

Oil is a type of fluid pixel. Oil flows similarly to water but is flammable. Oil burns out rather quickly when ignited. Oil will produce massive smoke when burning.

### Stone

Stone is a type of solid pixel. Stone is immovable and indestructible. It serves as the primary building block for creating structures in the game world.

### Wood

Wood is a type of solid pixel. Wood can be burned when ignited. Wood burns for a moderate amount of time and produces a little smoke.

### Copper

Copper is a type of solid pixel. Copper conducts electricity and can be used to create simple circuits in the game world. Futhermore, copper is extremely good at conducting heat, compared to other pixels.

### Sand

Sand is a type of solid pixel. Sand falls downwards due to gravity and can pile up.

### TNT

Note: this pixel is not yet implemented.

TNT is a type of solid pixel. TNT will explode when ignited.

### Lithium

Note: this pixel is not yet implemented.

Lithium is a type of solid pixel. Lithium explodes when it comes into contact with water or steam. Lithium will randomly turn into Lithium Oxide when exposed to air. Lithium falls downwards due to gravity, similar to sand.

### Lithium Oxide

Note: this pixel is not yet implemented.

Lithium Oxide is a type of solid pixel. Lithium Oxide is the stable product of Lithium reacting with air. Lithium Oxide falls downwards due to gravity, similar to sand.

## Structures

### Laser Emitter

The Laser Emitter structure emits a continuous laser beam in a specified direction (up, down, left, or right) when powered by an electrical signal.

### Laser Receiver

The Laser Receiver structure detects incoming laser beams from a specified direction (up, down, left, or right) and outputs an electrical signal when hit by a laser.

### Pressure Plate

The Pressure Plate structure outputs an electrical signal when some fluid/solid pixels are on top of it. The signal is turned off when there are no pixels on top of it.

### Power Source

The Power Source structure outputs a constant electrical signal.

### Heater

The Heater structure produces heat when powered by an electrical signal.

### Gate

The Gate structure that can open or close a passage for the duck and fluid pixels. When powered by an electrical signal, the gate opens, allowing the duck and fluid pixels to pass through. When not powered, the gate closes, blocking passage.

### Heat Sensor

The Heat Sensor structure outputs an electrical signal when the temperature in its detection area exceeds a certain threshold. The signal is turned off when the temperature drops below the threshold.

### Water Tap

Constantsly releases water pixels. When powered by an electrical signal, it stops releasing water.

### The duck

The duck is the main character in Waveforge. The player cannot directly control the duck; instead, they must manipulate the environment to guide the duck to the level's checkpoint. The duck can respond the following stimuli:

- Gravity: The duck will fall downwards due to gravity.

- Fluid buoyancy: The duck will float on top of fluid pixels (e.g., water, oil).

- Solid collision: The duck cannot pass through solid pixels (e.g., stone, wood, sand).

- Fluid flow: The duck will be carried by the flow of fluid pixels.

- Steam jets: The duck will be propelled by steam pixels, allowing it to reach higher areas.

- Explosion knockback: The duck will be pushed away from explosions. (Note: this is not yet implemented.)
