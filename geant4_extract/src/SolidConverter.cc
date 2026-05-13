#include "SolidConverter.hh"

#include <G4Box.hh>
#include <G4DisplacedSolid.hh>
#include <G4GenericPolycone.hh>
#include <G4SubtractionSolid.hh>
#include <G4SystemOfUnits.hh>
#include <G4ThreeVector.hh>
#include <G4Tubs.hh>
#include <G4UnionSolid.hh>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>

#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

#include <TopLoc_Location.hxx>

#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <iostream>

// ============================================================
// helper: Geant4 transform -> OCC transform
// ============================================================

static gp_Trsf BuildTransform(
    const G4RotationMatrix& rot,
    const G4ThreeVector& trans
) {

    gp_Trsf trsf;

    double tx = trans.x() / mm;
    double ty = trans.y() / mm;
    double tz = trans.z() / mm;

    trsf.SetValues(

        rot.xx(),
        rot.xy(),
        rot.xz(),
        tx,

        rot.yx(),
        rot.yy(),
        rot.yz(),
        ty,

        rot.zx(),
        rot.zy(),
        rot.zz(),
        tz
    );

    return trsf;
}

// ============================================================
// dispatcher
// ============================================================

TopoDS_Shape SolidConverter::Convert(
    G4VSolid* solid
) {

    // --------------------------------------------------------
    // box
    // --------------------------------------------------------

    if (auto* box =
        dynamic_cast<G4Box*>(solid)) {

        double dx =
            2.0 * box->GetXHalfLength();

        double dy =
            2.0 * box->GetYHalfLength();

        double dz =
            2.0 * box->GetZHalfLength();

        gp_Pnt corner(
            -dx / 2.0,
            -dy / 2.0,
            -dz / 2.0
        );

        return BRepPrimAPI_MakeBox(
            corner,
            dx,
            dy,
            dz
        );
    }

    // --------------------------------------------------------
    // tubs
    // --------------------------------------------------------

    if (auto* tubs =
        dynamic_cast<G4Tubs*>(solid)) {

        double rmin =
            tubs->GetInnerRadius();

        double rmax =
            tubs->GetOuterRadius();

        double h =
            2.0 * tubs->GetZHalfLength();

        gp_Ax2 ax(
            gp_Pnt(0, 0, -h / 2.0),
            gp_Dir(0, 0, 1)
        );

        // outer cylinder
        TopoDS_Shape outer =
            BRepPrimAPI_MakeCylinder(
                ax,
                rmax,
                h
            );

        // solid cylinder
        if (rmin <= 0.0)
            return outer;

        // hollow cylinder
        TopoDS_Shape inner =
            BRepPrimAPI_MakeCylinder(
                ax,
                rmin,
                h
            );

        return BRepAlgoAPI_Cut(
            outer,
            inner
        );
    }

    // --------------------------------------------------------
    // generic polycone
    // --------------------------------------------------------

    if (auto* poly =
        dynamic_cast<G4GenericPolycone*>(solid)) {

        return ConvertGenericPolycone(poly);
    }

    // --------------------------------------------------------
    // union
    // --------------------------------------------------------

    if (auto* uni =
        dynamic_cast<G4UnionSolid*>(solid)) {

        return ConvertUnion(uni);
    }

    // --------------------------------------------------------
    // subtraction
    // --------------------------------------------------------

    if (auto* sub =
        dynamic_cast<G4SubtractionSolid*>(solid)) {

        return ConvertSubtraction(sub);
    }

    // --------------------------------------------------------
    // displaced solid
    // --------------------------------------------------------

    if (auto* disp =
        dynamic_cast<G4DisplacedSolid*>(solid)) {

        return ConvertDisplaced(disp);
    }

    // --------------------------------------------------------
    // unsupported
    // --------------------------------------------------------

    std::cout
        << "Unsupported solid type: "
        << solid->GetEntityType()
        << std::endl;

    return TopoDS_Shape();
}

// ============================================================
// generic polycone
// ============================================================

TopoDS_Shape SolidConverter::ConvertGenericPolycone(
    G4GenericPolycone* solid
) {

    int n =
        solid->GetNumRZCorner();

    if (n < 2)
        return TopoDS_Shape();

    BRepBuilderAPI_MakeWire wireBuilder;

    for (int i = 0; i < n - 1; ++i) {

        auto p1g =
            solid->GetCorner(i);

        auto p2g =
            solid->GetCorner(i + 1);

        gp_Pnt p1(
            p1g.r / mm,
            0.0,
            p1g.z / mm
        );

        gp_Pnt p2(
            p2g.r / mm,
            0.0,
            p2g.z / mm
        );

        auto edge =
            BRepBuilderAPI_MakeEdge(
                p1,
                p2
            );

        wireBuilder.Add(edge);
    }

    TopoDS_Wire wire =
        wireBuilder.Wire();

    gp_Ax1 axis(
        gp_Pnt(0,0,0),
        gp_Dir(0,0,1)
    );

    return BRepPrimAPI_MakeRevol(
        wire,
        axis,
        2.0 * M_PI
    );
}

// ============================================================
// union
// ============================================================

TopoDS_Shape SolidConverter::ConvertUnion(
    G4UnionSolid* solid
) {

    auto left =
        Convert(
            solid->GetConstituentSolid(0)
        );

    auto right =
        Convert(
            solid->GetConstituentSolid(1)
        );

    if (left.IsNull() || right.IsNull())
        return TopoDS_Shape();

    return BRepAlgoAPI_Fuse(
        left,
        right
    );
}

// ============================================================
// subtraction
// ============================================================

TopoDS_Shape SolidConverter::ConvertSubtraction(
    G4SubtractionSolid* solid
) {

    auto left =
        Convert(
            solid->GetConstituentSolid(0)
        );

    auto right =
        Convert(
            solid->GetConstituentSolid(1)
        );

    if (left.IsNull() || right.IsNull())
        return TopoDS_Shape();

    return BRepAlgoAPI_Cut(
        left,
        right
    );
}

// ============================================================
// displaced solid
// ============================================================

TopoDS_Shape SolidConverter::ConvertDisplaced(
    G4DisplacedSolid* solid
) {

    auto* child =
        solid->GetConstituentMovedSolid();

    auto shape =
        Convert(child);

    if (shape.IsNull())
        return TopoDS_Shape();

    gp_Trsf trsf =
        BuildTransform(

            solid->GetObjectRotation(),

            solid->GetObjectTranslation()
        );

    return shape.Moved(
        TopLoc_Location(trsf)
    );
}
