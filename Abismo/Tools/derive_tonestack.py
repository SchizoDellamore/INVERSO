"""Derives the transfer function of the classic treble/bass/middle tone stack
(Fender and Marshall amps share this topology, with different parts) by nodal
analysis, and prints C++ for its analog coefficients.

Nodes: vin (driven), A (top of treble pot), O (treble wiper = output),
B (bottom of treble pot = top of bass pot), C (bottom of bass pot = mid node),
N (slope resistor node).
  C1: vin-A     treble pot: A-O (1-t)R1, O-B t R1     bass pot: B-C l R2
  mid pot: C-ground m R3     R4: vin-N     C2: N-B     C3: N-C     load RL: O-ground
"""
import sympy as sp

s, t, l, m = sp.symbols('s t l m')
R1, R2, R3, R4, RL, C1, C2, C3 = sp.symbols('R1 R2 R3 R4 RL C1 C2 C3', positive=True)
vin = sp.Symbol('vin')
A, O, B, C, N = sp.symbols('A O B C N')
Y = lambda r: 1 / r
Ycap = lambda c: s * c
eps = 0  # pots are kept away from 0 by the caller
Ra, Rb, Rbass, Rmid = (1 - t) * R1, t * R1, l * R2, m * R3
eqs = [
    sp.Eq((A - vin) * Ycap(C1) + (A - O) * Y(Ra), 0),
    sp.Eq((O - A) * Y(Ra) + (O - B) * Y(Rb) + O * Y(RL), 0),
    sp.Eq((B - O) * Y(Rb) + (B - C) * Y(Rbass) + (B - N) * Ycap(C2), 0),
    sp.Eq((C - B) * Y(Rbass) + C * Y(Rmid) + (C - N) * Ycap(C3), 0),
    sp.Eq((N - vin) * Y(R4) + (N - B) * Ycap(C2) + (N - C) * Ycap(C3), 0),
]
sol = sp.solve(eqs, [A, O, B, C, N], dict=True)[0]
H = sp.together(sp.simplify(sol[O] / vin))
num, den = sp.fraction(H)
num = sp.Poly(sp.expand(num), s)
den = sp.Poly(sp.expand(den), s)
nc = num.all_coeffs()[::-1]
dc = den.all_coeffs()[::-1]
order = max(len(nc), len(dc)) - 1
print(f'// order {order}: numerator degree {len(nc) - 1}, denominator degree {len(dc) - 1}')
norm = dc[0]
for name, coeffs in (('b', nc), ('a', dc)):
    for i, c in enumerate(coeffs):
        e = sp.simplify(sp.expand(c / norm))
        print(f'{name}[{i}] = {sp.ccode(e)};')

# numeric check against a direct solve at one setting
vals = {R1: 250e3, R2: 1e6, R3: 25e3, R4: 56e3, RL: 1e6, C1: 250e-12, C2: 20e-9, C3: 20e-9, t: .5, l: .5, m: .5}
import numpy as np
f = 1000.0
Hn = complex(H.subs(vals).subs(s, 2j * np.pi * f))
M = sp.Matrix([[sp.diff(e.lhs, v) for v in (A, O, B, C, N)] for e in eqs])
rhs = sp.Matrix([-e.lhs.subs({A: 0, O: 0, B: 0, C: 0, N: 0}) for e in eqs])
x = (M.subs(vals).subs(s, 2j * np.pi * f).subs(vin, 1)).evalf().LUsolve(rhs.subs(vals).subs(s, 2j * np.pi * f).subs(vin, 1).evalf())
print('// check at 1 kHz, t=l=m=.5: formula', abs(Hn), 'direct', abs(complex(x[1])))
