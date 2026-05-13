import pyvista as pv

# --------------------------------------------------
# load STL
# --------------------------------------------------

mesh = pv.read("interface_0.stl")

# --------------------------------------------------
# basic info
# --------------------------------------------------

print(mesh)

print("Number of points :", mesh.n_points)
print("Number of faces  :", mesh.n_cells)

print("Bounds:")
print(mesh.bounds)

print("Is manifold :", mesh.is_manifold)

# --------------------------------------------------
# viewer
# --------------------------------------------------

plotter = pv.Plotter()

# surface rendering
plotter.add_mesh(
    mesh,
    style="wireframe",
    color="white"
)
# coordinate axes
plotter.add_axes()

# grid
plotter.show_grid()

plotter.show()
