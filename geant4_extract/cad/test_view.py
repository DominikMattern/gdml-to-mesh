import cadquery as cq
import pyvista as pv
import numpy as np

# --------------------------------------------------
# load BREP
# --------------------------------------------------

shape = cq.importers.importBrep("detector.brep")

# --------------------------------------------------
# tessellate
# --------------------------------------------------

vertices, triangles = shape.val().tessellate(0.1)

# --------------------------------------------------
# convert cadquery vectors -> numpy
# --------------------------------------------------

vertices = np.array([
    [v.x, v.y, v.z]
    for v in vertices
], dtype=float)

triangles = np.array(
    triangles,
    dtype=np.int64
)

# --------------------------------------------------
# pyvista face format
# --------------------------------------------------

faces = np.hstack([
    np.full((triangles.shape[0], 1), 3),
    triangles
])

faces = faces.flatten()

# --------------------------------------------------
# create mesh
# --------------------------------------------------

mesh = pv.PolyData(
    vertices,
    faces
)

# --------------------------------------------------
# viewer
# --------------------------------------------------

plotter = pv.Plotter()

plotter.add_mesh(
    mesh,
    color="lightblue",
    opacity=0.5,
    show_edges=True
)

plotter.add_axes()

plotter.show_grid()

plotter.show()
