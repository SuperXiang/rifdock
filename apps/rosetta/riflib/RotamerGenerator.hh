// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://wsic_dockosettacommons.org. Questions about this casic_dock
// (c) addressed to University of Waprotocolsgton UW TechTransfer, email: license@u.washington.eprotocols
#ifndef INCLUDED_riflib_RotamerGenerator_hh
#define INCLUDED_riflib_RotamerGenerator_hh

#include <riflib/types.hh>
#include <core/types.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/pack/rotamer_set/RotamerSet.fwd.hh>
#include <scheme/actor/Atom.hh>
#include <scheme/chemical/RotamerIndex.hh>
#include <numeric/xyzVector.hh>
#include <vector>
#include <Eigen/Dense>
#include <riflib/rifdock_typedefs.hh>

namespace devel {
namespace scheme {

using ::scheme::chemical::HBondRay;


struct RosettaRotamerGenerator {
	typedef SchemeAtom Atom;

	void
	get_atoms( // TODO: possibly replace with fill_rotamer( scheme rot )
		std::string resname,
		std::vector<float> const & chi,
		std::vector<SchemeAtom> & atoms,
		std::vector<std::pair<SchemeAtom,SchemeAtom> > & hbonders, // TODO: depricate this?
		int & nheavyatoms,
		std::vector<HBondRay> & donors,
		std::vector<HBondRay> & acceptors,
		std::map<std::string, std::string> const & d_l_map
	);

};

std::set<std::pair<int,int>>
get_satisfied_atoms(core::pose::Pose const & pose, float ethresh=-0.1);

// makes copy of pose to score
std::vector<std::pair<int,int>>
get_satisfied_atoms_vector(core::pose::Pose pose, float ethresh=-0.1);

struct HBRayOpts {
	bool withbb = true;
	bool lkball = true;
	bool add_acceptor_mid = false;
	float orblen = ::scheme::chemical::ORBLEN;			// don't change this number!!!!!!!!!!
	std::set<std::pair<int,int>> satisfied_atoms;
};

void get_donor_rays   ( core::pose::Pose const & pose, int ir, HBRayOpts const & opt, std::vector<HBondRay> & donors );
void get_acceptor_rays( core::pose::Pose const & pose, int ir, HBRayOpts const & opt, std::vector<HBondRay> & acceptors );

void get_donor_rays   ( core::pose::Pose const & pose, int ir, HBRayOpts const & opt,
	std::vector<HBondRay> & donors,std::vector<std::pair<int,std::string>> & anames );
void get_acceptor_rays( core::pose::Pose const & pose, int ir, HBRayOpts const & opt,
	std::vector<HBondRay> & acceptors, std::vector<std::pair<int,std::string>> & anames );
void get_acceptor_rays_lkball( core::pose::Pose const & pose, int ir, HBRayOpts const & opt,
	std::vector<HBondRay> & acceptors, std::vector<std::pair<int,std::string>> & anames );

void dump_hbond_rays( std::ostream & out, std::vector<HBondRay> hbonders, bool isdonor,
						EigenXform const & xform = EigenXform::Identity() );

std::vector<HBondRay> load_hbond_rays( std::string fname );

core::pack::rotamer_set::RotamerSetOP
get_rosetta_rot_set(
	core::pose::Pose & pose,
	core::Size ir
 );
void
get_rotamer_spec_default(
	::scheme::chemical::RotamerIndexSpec & rot_index,
	bool extra_rotamers,
	bool extra_primary_rotamers,
	bool use_d_aa,
	bool use_l_aa
);

shared_ptr<RotamerIndex>
get_rotamer_index(
	::scheme::chemical::RotamerIndexSpec const & rot_index,
	bool build_per_thread_rotamers
);

std::shared_ptr<RotamerIndex>
get_rotamer_index(
	std::string cachefile,
	bool build_per_thread_rotamers,
	::scheme::chemical::RotamerIndexSpec & rot_index_spec
);


}}

#endif
