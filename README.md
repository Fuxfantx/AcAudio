# Aerials Audio System

A `miniaudio` binding for `Defold Engine`. Only APIs used in `Aerials` are implemented.

**Refer to** `api/acaudio.script_api` for API usages.

---

### Example

You should implement your HitSound system **with a Unit Pool** like this:

```lua
local CreateUnit = AcAudio.CreateUnit
local PlayUnit = AcAudio.PlayUnit

-- 1.Create a HitSound Resource (some_buf is a Defold Buffer)
--
local ResourceCreated, HitSoundRes = AcAudio.CreateResource(some_buf)

-- 2.Create the HitSound Unit Pool
--
local HitSoundUnits, UnitCount
if ResourceCreated then
    local UnitCreated, FirstUnit, UnitLen = CreateUnit(HitSoundRes)
    if UnitCreated then
        HitSoundUnits = {FirstUnit}
        --
        -- Choose a reasonable filter interval,
        -- And set the UnitCount according to it.
        --
        UnitCount = math.ceil(UnitLen / 50)   -- Filter Interval: 50ms
        for i=2,UnitCount do
            local ok, u, l = CreateUnit(HitSoundRes)
            if ok then
                HitSoundUnits[#HitSoundUnits+1] = u
            end
        end
        UnitCount = #HitSoundUnits
    end
end

-- 3.Encapsule the Playing Method with the Unit Pool Created above
--
local Which = 1
local function hit()
    PlayUnit(HitSoundUnits[Which], false)
    Which = Which + 1
    if Which > UnitCount then
        Which = 1
    end
end
```

---

### Complying with Licenses Related

- The extension part is under the `MIT` License.

- For the `miniaudio` library included, the `MIT-0` License is chosen here:
  
  Copyright 2023 David Reid
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

- See also: [miniaudio - A single file audio playback and capture library.](https://miniaud.io/index.html)
