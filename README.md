# nuclearcraft-optimizer
NuclearCraft (pre 2.0 rewrite) reactor optimizer. 

## Block codes

| Code | Block name |
| :-: | :--- |
| `[]` | Fuel Cell Compartment |
| `##` | Graphite Block |
| `Sp` | Reactor Speed Block |
| `St` | Reactor Cooler |
| `Wt` | Water Reactor Cooler |
| `Cr` | Cryotheum Reactor Cooler |
| `Rs` | Redstone Reactor Cooler |
| `Ed` | Resonant Reactor Cooler |
| `Gl` | Glowstone Reactor Cooler |
| `Cl` | Coolant Reactor Cooler |
| `He` | Liquid Helium Reactor Cooler |
| `..` | Air |
| `XX` | Reactor Casing |


## Compilation

```
gcc -O3 -march=native main.c -lm -o reactor
```