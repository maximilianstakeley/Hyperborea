"""Rev16 differential input model. Run with Python 3, no external dependencies."""
import itertools
import math

def leg(f, electrode, corners):
    rs, cc, rb, rf, cm, cable = corners
    # Deliberately opposite component tolerances for the two input branches.
    jw = 2j * math.pi * f
    cable_y = jw * cable
    source_gain = 1/(1 + electrode*cable_y)
    series = electrode*source_gain + 540e3 * (1 + rs * .01) + 1 / (jw * 100e-9 * (1 + cc * .05))
    bias = 10e6 * (1 + rb * .01)
    gain = bias / (series + bias)
    z = 100e3 * (1 + rf * .001) + series * bias / (series + bias)
    return gain*source_gain, z, jw * 10e-12 * (1 + cm * .01)

def response(f, first, second, common):
    g1, z1, y1 = first
    g2, z2, y2 = second
    yd = 2j * math.pi * f * 330e-12
    a, b = 1/z1 + y1 + yd, 1/z2 + y2 + yd
    i1 = g1/z1 * (1 if common else .5)
    i2 = g2/z2 * (1 if common else -.5)
    return ((b-yd)*i1 - (a-yd)*i2)/(a*b-yd*yd)

if __name__ == '__main__':
    for f in (0.5, 50, 60, 100):
        corners = list(itertools.product(*[(-1,1)]*5, (0,100e-12)))
        legs = [leg(f,e,c) for e in (0,1e4) for c in corners]
        cases = list(itertools.product(legs, repeat=2))
        cm = max(abs(response(f,*c,True)) for c in cases)
        dm = min(abs(response(f,*c,False)) for c in cases)
        print(f'{f:g} Hz: min differential gain {dm:.4f}, worst modeled common gain {cm:.6f}, CMRR >= {20*math.log10(dm/cm):.1f} dB over sampled cases')
    print('Resistor/electrode thermal estimate, 313 K, 100 Hz:', math.sqrt(4*1.380649e-23*313*1.30e6*100)*1e6,'uV RMS')
