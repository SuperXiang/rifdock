// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

#include <riflib/util.hh>
#include <riflib/scaffold/util.hh>
#include <riflib/rosetta_field.hh>
#include <riflib/scaffold/MultithreadPoseCloner.hh>
#include <numeric/agglomerative_hierarchical_clustering.hh>
#include <scheme/chemical/stub.hh>

#include <ObjexxFCL/format.hh>
#include <core/id/AtomID.hh>
#include <core/id/AtomID_Map.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/conformation/util.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/methods/EnergyMethodOptions.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/sasa.hh>
#include <core/scoring/hbonds/hbonds.hh>
#include <core/scoring/hbonds/HBondSet.hh>
#include <core/scoring/hbonds/HBondOptions.hh>
#include <utility/file/file_sys_util.hh>
#include <core/scoring/dssp/Dssp.hh>
#include <utility/io/izstream.hh>
#include <utility/io/ozstream.hh>


#include <core/scoring/rms_util.hh>
#include <core/select/util.hh>

#include <boost/functional/hash/hash.hpp>


namespace devel {
namespace scheme {

void
add_res_if_not_CGP(
	core::pose::Pose const & pose,
	int ir,
	utility::vector1<core::Size> & res,
	bool verbose = false
){
	if( pose.residue(ir).name3() == "CYD" ){ if(verbose) std::cout << "get_res skip CYD" << std::endl; return; }
	if( pose.residue(ir).name3() == "GLY" ){ if(verbose) std::cout << "get_res skip GLY" << std::endl; return; }
	if( pose.residue(ir).name3() == "PRO" ){ if(verbose) std::cout << "get_res skip PRO" << std::endl; return; }
	res.push_back( ir );
}

utility::vector1<core::Size>
get_res(
	std::string fname,
	core::pose::Pose const & pose,
	bool nocgp
 ){
 	using core::Size;
	utility::vector1<core::Size> res;
	if(fname==""){
		for(Size ir = 1; ir <= pose.size(); ++ir){
			if( nocgp )	add_res_if_not_CGP( pose, ir, res );
			else res.push_back(ir);
		}
		return res;
	}
	runtime_assert_msg( utility::file::file_exists( fname ), "res list file doesn't exist: " + fname );
	utility::io::izstream in(fname);
	std::string s;
	while(in>>s){
		try {
			utility::vector1<std::string> splt = utility::string_split(s,'-');
			// cout << splt << endl;
			if(splt.size()==1){
				// std::cout << "'" << splt.front() << "'" << std::endl;
				Size ir = boost::lexical_cast<Size>(splt.front());
				runtime_assert_msg( ir > 0 && ir <= pose.size(), "residue number out of bounds "+boost::lexical_cast<std::string>(ir) );
				if( nocgp )	add_res_if_not_CGP( pose, ir, res );
				else res.push_back(ir);
			}
			else if(splt.size()==2){
				Size lb = boost::lexical_cast<Size>(splt.front());
				Size ub = boost::lexical_cast<Size>(splt.back());
				if( ub < lb ) std::swap(lb,ub);
				for(Size ir = lb; ir <= ub; ++ir){
					runtime_assert_msg( ir > 0 && ir <= pose.size(), "residue number out of bounds "+boost::lexical_cast<std::string>(ir) );
				if( nocgp )	add_res_if_not_CGP( pose, ir, res );
				else res.push_back(ir);
				}
			} else {
				utility_exit_with_message("get_res: can't parse res line "+s);
			}
		} catch( boost::bad_lexical_cast ){
			utility_exit_with_message("get_res: can't parse res line "+s);
		}
	}
	in.close();

	std::set<core::Size> uniq;
	std::copy(res.begin(), res.end(), std::inserter(uniq, uniq.begin()));
	res.clear();
	std::copy(uniq.begin(), uniq.end(), std::back_inserter(res));
	std::sort( res.begin(), res.end() );
	if( nocgp ){
		for( int iri = 1; iri <= res.size(); ++iri ){
			int ir = res[iri];
			// std::cout << "selected res " << ir << " " << pose.residue(ir).name3() << std::endl;
			runtime_assert( pose.residue(ir).name3() != "CYD" );
			runtime_assert( pose.residue(ir).name3() != "GLY" );
			runtime_assert( pose.residue(ir).name3() != "PRO" );
		}
	}
	// utility_exit_with_message("oriestnd");

	return res;
 }

utility::vector1<core::Size> get_designable_positions_best_guess(
	  core::pose::Pose pose
	, bool noloops
	, bool nocgp
 ){
 	core::scoring::dssp::Dssp dssp( pose );
 	dssp.insert_ss_into_pose( pose );

 	core::pose::Pose allgly = pose;
	core::pose::Pose allval = pose;
 	pose_to_gly(allgly);
	pose_to_val(allval);

 	core::scoring::ScoreFunctionOP sf = core::scoring::get_score_function(true);
 	sf->score(allgly);

	core::scoring::methods::EnergyMethodOptions myopt = sf->energy_method_options();
	myopt.hbond_options().decompose_bb_hb_into_pair_energies(true);
	sf->set_energy_method_options(myopt);
 	sf->score(pose);
	core::scoring::hbonds::HBondSet hbset;
	core::scoring::hbonds::fill_hbond_set(pose,false,hbset);

	std::vector<float> sc2bb_energy(pose.size(),false);
	for(core::Size ihb = 1; ihb <= hbset.nhbonds(); ++ihb){
		core::scoring::hbonds::HBond const & hb(hbset.hbond(ihb));
		// noly donot design capping residues???
		if(  hb.don_hatm_is_protein_backbone() && !hb.acc_atm_is_protein_backbone() && pose.secstruct( hb.acc_res() ) == 'L' ) sc2bb_energy[hb.acc_res()-1] += hb.energy();
		if( !hb.don_hatm_is_protein_backbone() &&  hb.acc_atm_is_protein_backbone() && pose.secstruct( hb.don_res() ) == 'L' ) sc2bb_energy[hb.don_res()-1] += hb.energy();
	}

	utility::vector1<core::Size> res;
	// Real calc_per_atom_sasa( pose::Pose const & pose, id::AtomID_Map< Real > & atom_sasa, utility::vector1< Real > & rsd_sasa,
	// Real const probe_radius, bool const use_big_polar_H = false );
	core::id::AtomID_Map< core::Real > atom_sasa;
	utility::vector1< core::Real > rsd_sasa;
	core::scoring::calc_per_atom_sasa( allval, atom_sasa, rsd_sasa, 2.1 );
	for( int ir = 1; ir <= pose.size(); ++ir ){
		// std::cout << pose.secstruct(ir) << std::endl;
		bool isloop = pose.secstruct(ir) == 'L';
		int natoms = pose.residue(ir).nheavyatoms()-pose.residue(ir).last_backbone_atom();
		core::Real scsasa = 0;
		for( int ia = pose.residue(ir).first_sidechain_atom(); ia <= pose.residue(ir).natoms(); ++ia ){
			scsasa += atom_sasa[core::id::AtomID(ia,ir)];
		}
		float scsasa_per_atom = scsasa / float(natoms);

		bool is_exposed = scsasa_per_atom > 5.5;
		is_exposed |= scsasa > 18.0;
		// Longxing changed the cut off value below to -0.6, as most of the real capping Hbond score is lower than -1.0.
		bool has_bbsc = sc2bb_energy[ir-1] < -0.6;
		// if(pose.residue(ir).nchi()==4) has_bbsc = false;

		float datr =    pose.energies().residue_total_energies(ir)[core::scoring::fa_atr]
		             -allgly.energies().residue_total_energies(ir)[core::scoring::fa_atr];
		// float dhb  =    pose.energies().residue_total_energies(ir)[core::scoring::hbond_sc]
		//              -allgly.energies().residue_total_energies(ir)[core::scoring::hbond_sc];
		//       dhb +=    pose.energies().residue_total_energies(ir)[core::scoring::fa_elec]
		//              -allgly.energies().residue_total_energies(ir)[core::scoring::fa_elec];

		bool lowenergy = false;
		// lowenergy |= (( datr ) / float(natoms+1)) < -0.9;
		// lowenergy |= ( dhb  ) < -1.8;
		lowenergy |= has_bbsc;

		bool design_res = !lowenergy && is_exposed;

		if(noloops) design_res = design_res & !isloop;

		if( design_res ){
			if( nocgp )	add_res_if_not_CGP( pose, ir, res );
			else res.push_back(ir);
		} else {
			// std::cout << "pruned res by sasa " << ir << std::endl;
		}
	}
	runtime_assert_msg( res.size() > 0, "no residues selected!" );
	std::cout << "get_designable_positions_best_guess keep " << res.size() << " of " << pose.size() << std::endl;
	std::cout << "select designable_positions_best_guess=resi " << res[1];
	for(int i = 2; i <= res.size(); ++i ) std::cout << "+"<<res[i];
	std::cout << std::endl;

	// utility_exit_with_message("foo");

	return res;
}

std::string
get_res_list_hash( utility::vector1<core::Size> const & reslist ){
	assert( reslist.size() > 0 );
	std::size_t reslist_hash = boost::hash_range( reslist.begin(), reslist.end() );
	std::string hashstr = boost::lexical_cast<std::string>(reslist_hash);
	return hashstr.substr(hashstr.size()-8,8);

}



std::string KMGT(double const & x, int const & w, int const & d){
	using ObjexxFCL::format::F;
	if( x < 1e3  ) return F( w, d, x/1e0  )+" ";
	if( x < 1e6  ) return F( w, d, x/1e3  )+"K";
	if( x < 1e9  ) return F( w, d, x/1e6  )+"M";
	if( x < 1e12 ) return F( w, d, x/1e9  )+"G";
	if( x < 1e15 ) return F( w, d, x/1e12 )+"T";
	if( x < 1e18 ) return F( w, d, x/1e15 )+"P";
	if( x < 1e21 ) return F( w, d, x/1e18 )+"E";
	if( x < 1e24 ) return F( w, d, x/1e21 )+"Z";
	else           return F( w, d, x/1e24 )+"Y";

}

void pose_to_ala( core::pose::Pose & pose ){
	core::chemical::ResidueTypeSetCAP rts = core::chemical::ChemicalManager::get_instance()->residue_type_set("fa_standard");
	core::conformation::ResidueOP ala = core::conformation::ResidueFactory::create_residue( rts.lock()->name_map("ALA") );
	for( int ir = 1; ir <= pose.size(); ++ir ){
		if( ! pose.residue(ir).is_protein()   ) continue;
		if(   pose.residue(ir).name3()=="GLY" ) continue;
		if(   pose.residue(ir).name3()=="PRO" ) continue;
		if(   pose.residue(ir).name3()=="CYD" ) continue;
		pose.replace_residue( ir, *ala, true );
	}
}
void pose_to_gly( core::pose::Pose & pose ){
	core::chemical::ResidueTypeSetCAP rts = core::chemical::ChemicalManager::get_instance()->residue_type_set("fa_standard");
	core::conformation::ResidueOP gly = core::conformation::ResidueFactory::create_residue( rts.lock()->name_map("GLY") );
	for( int ir = 1; ir <= pose.size(); ++ir ){
		if( ! pose.residue(ir).is_protein()   ) continue;
		if(   pose.residue(ir).name3()=="GLY" ) continue;
		if(   pose.residue(ir).name3()=="PRO" ) continue;
		if(   pose.residue(ir).name3()=="CYD" ) continue;
		pose.replace_residue( ir, *gly, true );
	}
}
void pose_to_ala( core::pose::Pose & pose, utility::vector1<core::Size> const & res_sel ){
	core::chemical::ResidueTypeSetCAP rts = core::chemical::ChemicalManager::get_instance()->residue_type_set("fa_standard");
	core::conformation::ResidueOP ala = core::conformation::ResidueFactory::create_residue( rts.lock()->name_map("ALA") );
	for( int iri = 1; iri <= res_sel.size(); ++iri ){
		int ir = res_sel[iri];
		if( ! pose.residue(ir).is_protein()   ) continue;
		if(   pose.residue(ir).name3()=="GLY" ) continue;
		if(   pose.residue(ir).name3()=="PRO" ) continue;
		if(   pose.residue(ir).name3()=="CYD" ) continue;
		pose.replace_residue( ir, *ala, true );
	}
}
void pose_to_val( core::pose::Pose & pose ){
	core::chemical::ResidueTypeSetCAP rts = core::chemical::ChemicalManager::get_instance()->residue_type_set("fa_standard");
	core::conformation::ResidueOP val = core::conformation::ResidueFactory::create_residue( rts.lock()->name_map("VAL") );
	for( int ir = 1; ir <= pose.size(); ++ir ){
		if( ! pose.residue(ir).is_protein()   ) continue;
		if(   pose.residue(ir).name3()=="GLY" ) continue;
		if(   pose.residue(ir).name3()=="PRO" ) continue;
		if(   pose.residue(ir).name3()=="CYD" ) continue;
		pose.replace_residue( ir, *val, true );
	}
}

std::string
open_for_read_on_path(
	std::vector<std::string> const & path,
	std::string fname,
	utility::io::izstream & in
){
	for( auto const & dir : path ){
		if( utility::file::file_exists( dir+"/"+fname ) ){
			in.close();
			in.clear();
			in.open( dir+"/"+fname, std::ios::binary );
			if( in.good() ){
				// std::cout << "open_for_read_on_path: opened " << dir + "/" + fname << std::endl;
				return dir+"/"+fname;
			}
		}
	}
	return std::string();
}

std::string
open_for_write_on_path(
	std::vector<std::string> const & path,
	std::string fname,
	utility::io::ozstream & out,
	bool create_directorys /* = false */
){
	for( auto const & dir: path ){
		if( create_directorys && !utility::file::file_exists( dir ) ){
			utility::file::create_directory_recursive( dir );
		}
		if( utility::file::file_exists( dir ) ){
			out.close();
			out.clear();
			out.open( dir+"/"+fname, std::ios::binary );
			if( out.good() ){
				// std::cout << "open_for_write_on_path: opened " << dir + "/" + fname << std::endl;
				return dir+"/"+fname;
			}
		}
	}
	return std::string();

}




void
append_pose_to_pose(
	core::pose::Pose & pose1,
	core::pose::Pose const & pose2,
	bool new_chain
){
	append_subpose_to_pose(pose1, pose2, 1, pose2.size(), new_chain);
}



void
append_subpose_to_pose(
	core::pose::Pose & pose1,
	core::pose::Pose const & pose2,
	core::Size start_res,
	core::Size end_res,
	bool new_chain
){
	if ( pose2.size()<start_res ) {
		std::cerr << "Provided starting residue number " << start_res
			<< " less than number residues in appended pose. Nothing to do." << std::endl;
	}
	pose1.append_residue_by_jump(pose2.residue(start_res), pose1.size() , "", "", new_chain);
	for ( core::Size i=start_res+1; i<=end_res; ++i ) {
		if ( pose2.residue(i).is_lower_terminus() || !pose2.residue(i).is_protein() ) {
			if ( i > 1 && pose2.chain(i) == pose2.chain(i-1) ) {
				pose1.append_residue_by_jump(pose2.residue(i), pose1.size(), "","", false);
			} else {
				pose1.append_residue_by_jump(pose2.residue(i), pose1.size(), "","", true);
			}
		} else {
			pose1.append_residue_by_bond(pose2.residue(i));
		}
	}
}




std::vector<int>
get_rif_atype_map()
{
	core::chemical::AtomTypeSetCOP ats = core::chemical::ChemicalManager::get_instance()->atom_type_set("fa_standard");
	// 147 CH0
	// 148 HS
	// 149 NtrR
	// 150 SH1


	std::vector<int> atypemap;
	atypemap.resize( ats->n_atomtypes()+1, 12345 );
	atypemap[ ats->atom_type_index("CNH2") ] =  1;
	atypemap[ ats->atom_type_index("COO" ) ] =  2;
	atypemap[ ats->atom_type_index("CH1" ) ] =  3;
	atypemap[ ats->atom_type_index("CH2" ) ] =  4;
	atypemap[ ats->atom_type_index("CH3" ) ] =  5;
	atypemap[ ats->atom_type_index("aroC") ] =  6;
	atypemap[ ats->atom_type_index("Ntrp") ] =  7;
	atypemap[ ats->atom_type_index("Nhis") ] =  8;
	atypemap[ ats->atom_type_index("NH2O") ] =  9;
	atypemap[ ats->atom_type_index("Nlys") ] = 10;
	atypemap[ ats->atom_type_index("Narg") ] = 11;
	atypemap[ ats->atom_type_index("Npro") ] = 12;
	atypemap[ ats->atom_type_index("OH"  ) ] = 13;
	atypemap[ ats->atom_type_index("ONH2") ] = 14;
	atypemap[ ats->atom_type_index("OOC" ) ] = 15;
	atypemap[ ats->atom_type_index("Oaro") ] = 16;
	atypemap[ ats->atom_type_index("S"   ) ] = 17;
	atypemap[ ats->atom_type_index("Nbb" ) ] = 18;
	atypemap[ ats->atom_type_index("CAbb") ] = 19;
	atypemap[ ats->atom_type_index("CObb") ] = 20;
	atypemap[ ats->atom_type_index("OCbb") ] = 21;

	// -beta stuff
	if(ats->has_atom("CH0" )) atypemap[ ats->atom_type_index("CH0" ) ] = 6;
	// if(ats->has_atom("HS")) atypemap[ ats->atom_type_index("HS"  ) ] = ??;
	if(ats->has_atom("NtrR")) atypemap[ ats->atom_type_index("NtrR") ] = 11;
	if(ats->has_atom("SH1" )) atypemap[ ats->atom_type_index("SH1" ) ] = 17;

	return atypemap;
}



std::map< core::id::AtomID, core::id::AtomID >
residue_subset_to_CA_atom_map( 
    core::select::residue_selector::ResidueSubset const & subset, 
    core::pose::Pose const & pose ) {

    std::map< core::id::AtomID, core::id::AtomID > atom_map;
    utility::vector1<core::Size> seq_poss = core::select::get_residues_from_subset( subset );
    for ( core::Size seq_pos : seq_poss ) {
        core::conformation::Residue const & res( pose.residue( seq_pos ) );
        if ( res.has( "CA" ) ) {
            core::id::AtomID const id( res.atom_index( "CA" ), seq_pos );
            atom_map[ id ] = id;
        }
    }
    return atom_map;
}



core::Real
subset_CA_rmsd(
    core::pose::Pose const & pose1,
    core::pose::Pose const & pose2,
    core::select::residue_selector::ResidueSubset const & subset,
    bool superimpose) {

    utility::vector1<core::Size> calc_rms_res = core::select::get_residues_from_subset( subset );
    std::map< core::id::AtomID, core::id::AtomID > atom_map = 
        residue_subset_to_CA_atom_map( subset, pose1 );

    if ( superimpose ) {
        return core::scoring::rms_at_corresponding_atoms( pose1, pose2, atom_map, calc_rms_res );
    } else {
        return core::scoring::rms_at_corresponding_atoms_no_super( pose1, pose2, atom_map, calc_rms_res );
    }
}


/// Your poses must be identical. Very important
EigenXform
find_xform_from_identical_pose_to_pose( 
	core::pose::Pose const & pose1,
	core::pose::Pose const & pose2,
	float align_error /* = 0.2 */ ) {

	runtime_assert( pose1.size() == pose2.size() );

	// we can use anything here because they're the same
	utility::vector1<core::Size> scaffold_res;
	get_default_scaffold_res( pose1, scaffold_res );

	core::pose::Pose to_move = pose1;
    ::devel::scheme::pose_to_ala( to_move );
    Eigen::Vector3f to_move_center = pose_center(to_move, scaffold_res);

    core::pose::Pose match_this = pose2;
    ::devel::scheme::pose_to_ala( match_this );
    Eigen::Vector3f match_center = pose_center(match_this,scaffold_res);


   	// prepare all test point now

    // Test original residue 1
    utility::vector1<core::Size> target_res {1};
    std::vector< ::scheme::actor::Atom< Eigen::Vector3f > > to_move_atoms;
    std::vector< ::scheme::actor::Atom< Eigen::Vector3f > > match_atoms;
    
    devel::scheme::get_scheme_atoms( to_move, target_res, to_move_atoms, true );
    devel::scheme::get_scheme_atoms( match_this, target_res, match_atoms, true );


    // Test original midpoint
    core::Size test_res = to_move.size() / 2;

    utility::vector1<core::Size> test_target_res {test_res};
    std::vector< ::scheme::actor::Atom< Eigen::Vector3f > > test_to_move_atoms;
    std::vector< ::scheme::actor::Atom< Eigen::Vector3f > > test_match_atoms;

    devel::scheme::get_scheme_atoms( to_move, test_target_res, test_to_move_atoms, true );
    devel::scheme::get_scheme_atoms( match_this, test_target_res, test_match_atoms, true );


    // Test centered residue 1

    EigenXform to_move_2_center = EigenXform::Identity();
    to_move_2_center.translation() = -to_move_center;
    apply_xform_to_pose( to_move, to_move_2_center );

    std::vector< ::scheme::actor::Atom< Eigen::Vector3f > > to_move_centered_atoms;
    devel::scheme::get_scheme_atoms( to_move, target_res, to_move_centered_atoms, true );


    // done making test points




    EigenXform match_x = ::scheme::chemical::make_stub<EigenXform>(
                                                                match_atoms[0].position(),
                                                                match_atoms[1].position(),
                                                                match_atoms[2].position());
    EigenXform to_move_x = ::scheme::chemical::make_stub<EigenXform>(
                                                                to_move_centered_atoms[0].position(),
                                                                to_move_centered_atoms[1].position(),
                                                                to_move_centered_atoms[2].position());

    EigenXform to_move_centered_2_match = match_x * to_move_x.inverse();
    to_move_centered_2_match.translation() = match_center;

    double centered_error = (to_move_centered_2_match * to_move_centered_atoms[0].position() - match_atoms[0].position()).norm();
	std::cout << "Centered res1 Alignment error :" << centered_error << std::endl;
    runtime_assert( centered_error < align_error );

    EigenXform to_move_2_match = to_move_centered_2_match * to_move_2_center;

    double error = (to_move_2_match * to_move_atoms[0].position() - match_atoms[0].position()).norm();
	std::cout << "res1 Alignment error :" << error << std::endl;
    runtime_assert( error < align_error );


    double test_error = (to_move_2_match * test_to_move_atoms[0].position() - test_match_atoms[0].position()).norm();
    std::cout << "Test res" << test_res << " Alignment error :" << error << std::endl;
    runtime_assert( test_error < align_error );

    return to_move_2_match;
	
}



void
apply_xform_to_pose( core::pose::Pose & pose, EigenXform const & xform) {
	numeric::xyzTransform<float> transform = eigen2xyz( xform );
	pose.apply_transform_Rx_plus_v( transform.R, transform.t );

}

void
apply_xform_to_residue( core::conformation::Residue & residue, EigenXform const & xform) {
	numeric::xyzTransform<float> transform = eigen2xyz( xform );
	residue.apply_transform_Rx_plus_v( transform.R, transform.t );

}


void
all_by_all_rmsd( 
	std::vector<core::pose::PoseOP> const & poses,
	utility::vector1<utility::vector1<core::Real>> & table ) {
	
	table.clear();
	table.resize( poses.size() );

	for ( core::Size i = 1; i <= table.size(); i++ ) {
		table[i].resize(poses.size(), 0);	// initialize the diagonal to 0
	}

	// Make it threadsafe
	utility::vector1<shared_ptr<MultithreadPoseCloner>> mpcs;
	for ( core::pose::PoseOP const & pose : poses ) {
		mpcs.push_back(make_shared<MultithreadPoseCloner>(pose));
	}

	std::exception_ptr exception = nullptr;
    #ifdef USE_OPENMP
    #pragma omp parallel for schedule(dynamic,1)
    #endif
	for ( core::Size i = 1; i <= mpcs.size(); i++ ) {
		if (exception) continue;
		try {
			core::pose::PoseCOP outer_pose = mpcs[i]->get_pose();

			for ( core::Size j = i + 1; j <= mpcs.size(); j++) {
				core::pose::PoseCOP inner_pose = mpcs[j]->get_pose();

				core::Real rmsd = core::scoring::CA_rmsd(*outer_pose, *inner_pose);

				runtime_assert( table[i][j] == 0 );
				runtime_assert( table[j][i] == 0 );

				table[i][j] = rmsd;
				table[j][i] = rmsd;
			}

		} catch(...) {
            #pragma omp critical
            exception = std::current_exception();
        }

	} // end of OMP loop
    if( exception ) std::rethrow_exception(exception);

}

std::vector<std::vector<std::pair<core::pose::PoseOP, uint64_t>>>
cluster_poses_into_n_bins( 
	std::vector<core::pose::PoseOP> const & poses,
	uint64_t n,
	utility::vector1<utility::vector1<core::Real>> & rmsds ) {

	runtime_assert( poses.size() >= n );

	if (rmsds.size() != poses.size()) {
		std::cout << "Calculating n^2 rmsd table" << std::endl;
		all_by_all_rmsd( poses, rmsds );
	}

	numeric::AverageLinkClusterer alc;
	utility::vector1<numeric::ClusteringTreeNodeOP> roots = alc.cluster(rmsds, n);

	utility::vector1<uint64_t> input_indices( poses.size() );
	for ( core::Size i = 1; i <= poses.size(); i++ ) {
		input_indices[i] = i - 1;
	}

	std::vector<bool> pose_got_used(poses.size(), false);

	std::vector<std::vector<std::pair<core::pose::PoseOP, uint64_t>>> bins;

	for ( core::Size i = 1; i <= roots.size(); i++ ) {
		std::vector<std::pair<core::pose::PoseOP, uint64_t>> this_bin;

		utility::vector1<uint64_t> this_bin_indices;
		numeric::get_cluster_data( input_indices, roots[i], this_bin_indices );

		runtime_assert( this_bin_indices.size() > 0 );

		for ( uint64_t index : this_bin_indices ) {
			runtime_assert( ! pose_got_used[index] );
			pose_got_used[index] = true;
			this_bin.push_back( std::pair<core::pose::PoseOP, uint64_t>(poses[index], index) );
		}

		bins.push_back( this_bin );
	}

	for ( uint64_t i = 0; i < pose_got_used.size(); i++ ) {
		runtime_assert(pose_got_used[i]);
	}

	return bins;
}

std::vector<core::pose::PoseOP>
cluster_poses_leaving_n( 
	std::vector<core::pose::PoseOP> const & poses,
	uint64_t n ) {

	if ( n >= poses.size() ) return poses;

	utility::vector1<utility::vector1<core::Real>> rmsds;
	std::vector<std::vector<std::pair<core::pose::PoseOP, uint64_t>>> bins = cluster_poses_into_n_bins( poses, n, rmsds );

	std::vector<core::pose::PoseOP> output_poses;

	for (std::vector<std::pair<core::pose::PoseOP, uint64_t>> const & bin : bins) {
		output_poses.push_back(bin.front().first);
	}

	return output_poses;
}

// this returns the index into the indexes list
// indexes should be 0 based even though rmsds is 1 based
size_t
find_cluster_center( 
	std::vector<uint64_t> indexes,
	utility::vector1<utility::vector1<core::Real>> const & rmsds ) {

	std::vector<core::Real> rmsd_sums( indexes.size() );

	for ( size_t i = 0; i < indexes.size(); i++ ) {
		uint64_t index = indexes[i];

		core::Real this_sum = 0;
		for ( uint64_t ind : indexes ) {
			this_sum += rmsds.at(ind+1).at(index+1);
		}
		// std::cout << this_sum << std::endl;
		rmsd_sums[i] = this_sum;
	}
	ptrdiff_t center_index = std::min_element(rmsd_sums.begin(), rmsd_sums.end()) - rmsd_sums.begin();

	std::cout << "Cluster center is " << center_index << " with rmsd sum " << rmsd_sums[center_index] << std::endl;

	return center_index;
}

// uses a binary search to try to find a number
// of bins to cluster poses into such than the top n
// represent frac of poses within tol
std::vector<core::pose::PoseOP>
cluster_poses_leaving_n_representing_frac(
	std::vector<core::pose::PoseOP> const & poses,
	uint64_t n,
	float frac,
	float tol
	) {

	using ObjexxFCL::format::I;

	runtime_assert( frac > 0 && frac <= 1);

	uint64_t trial = n * (uint64_t)( 1.0 / frac );
	uint64_t initial_trial = trial;
	std::vector<uint64_t> trial_history;

	std::vector<size_t> idx ;
	utility::vector1<utility::vector1<core::Real>> rmsds;

	std::vector<std::vector<std::pair<core::pose::PoseOP, uint64_t>>> bins;

	while (true) {

		trial_history.push_back( trial );
		std::cout << "Round " << trial_history.size() << " : trial size " << trial << std::endl;

		bins = cluster_poses_into_n_bins( poses, trial, rmsds );

// sort indexes based on size of bin
		idx.clear();
		idx.resize(bins.size());
  		std::iota(idx.begin(), idx.end(), 0);
		std::sort(idx.begin(), idx.end(),
       		[&bins](size_t i1, size_t i2) {return bins[i1].size() < bins[i2].size();});


		size_t biggest_bin = bins[idx.back()].size();

// display block //////////////////////////
		{
			// find the number of digits
			int digits = 0; 
			{
				size_t temp = biggest_bin;
				while (temp != 0) { temp /= 10; digits++; }
			}

			size_t star_every = std::max<size_t>(biggest_bin / 50, 1);

			for ( size_t i = 0; i < bins.size(); i++ ) {
				if ( i == n ) std::cout << "============================ cut =========================" << std::endl;
				// going in reverse order and using indexes
				size_t bin = idx[ bins.size() - i - 1 ];

				std::cout << I( digits + 1, bins[bin].size() ) << " ";

				{
					int temp = bins[bin].size() - star_every;
					while ( temp > 0 ) { std::cout << "*"; temp -= star_every; }
				}
				std::cout << std::endl;
			}
		}

/////////////////////////////////////////////

// decide if we have met the tolerance criteria
		size_t current_count = 0;
		for ( size_t i = 0; i < n; i++ ) {
			size_t bin = idx[ bins.size() - i - 1 ];
			current_count += bins[bin].size();
		}

		float current_frac = (float)current_count / (float)poses.size();
		float error = current_frac - frac;
		std::cout << "Error = " << error << std::endl;
		if ( std::abs(error) < tol ) {
			std::cout << "Tolerance satisfied, clustering complete" << std::endl;
			break;
		}

// find position in array
		std::sort(trial_history.begin(), trial_history.end());
		ptrdiff_t pos = std::find(trial_history.begin(), trial_history.end(), trial ) - trial_history.begin();

// either one plus or one minus
		int other_relevant_pos;
		if ( error > 0 ) {
			other_relevant_pos = pos + 1;
		} else {
			other_relevant_pos = pos - 1;
		}

// naively set the next trial size
		if ( other_relevant_pos < 0 ) {
			trial /= 2;
		} else if ( other_relevant_pos >= trial_history.size() ) {
			trial *= 4;
		} else {
			uint64_t other_trial = trial_history[other_relevant_pos];
			trial = ( other_trial + trial ) / 2;
		}

// cap it to meaningful values
		trial = std::max<uint64_t>( 1, trial );
		trial = std::min<uint64_t>( poses.size(), trial );

// make sure next trial will be unique
		if ( std::find(trial_history.begin(), trial_history.end(), trial ) != trial_history.end() ) {
			std::cout << "Repeating trial, clustering complete" << std::endl;
			break;
		}
	}

	std::vector<core::pose::PoseOP> output_poses;

	for ( size_t i = 0; i < n; i++ ) {
		size_t bin = idx.at( bins.size() - i - 1 );
		std::vector<uint64_t> indexes;
		for ( std::pair<core::pose::PoseOP, uint64_t> pair : bins[bin] ) {
			indexes.push_back( pair.second );
		}
		size_t cluster_center = find_cluster_center( indexes, rmsds );
		output_poses.push_back( bins.at(bin).at(cluster_center).first );
	}

	return output_poses;
}

std::vector<core::pose::PoseOP>
random_selection_poses_leaving_n( 
	std::vector<core::pose::PoseOP> const & poses,
	uint64_t n ) {

	if ( n >= poses.size() ) return poses;

	std::vector<uint64_t> indexes( poses.size());
	for ( uint64_t i = 0; i < indexes.size(); i++ ) {
		indexes[i] = i;
	}

	std::random_shuffle(indexes.begin(), indexes.end());

	std::vector<core::pose::PoseOP> output_poses(n);

	for ( uint64_t i = 0; i < n; i++ ) {
		output_poses[i] = poses[indexes[i]];
	}

	return output_poses;
}




Eigen::Vector3f
pose_center(
    core::pose::Pose const & pose,
    utility::vector1<core::Size> const & useres /* = utility::vector1<core::Size>()*/
){
    typedef numeric::xyzVector<core::Real> Vec;
    Vec cen(0,0,0);
    int count = 0;
    for( int ir = 1; ir <= pose.size(); ++ir ) {
        if( useres.size()==0 || std::find(useres.begin(),useres.end(),ir)!=useres.end() ){
            for( int ia = 1; ia <= pose.residue(ir).nheavyatoms(); ++ia ){
                cen += pose.xyz(core::id::AtomID(ia,ir));
                ++count;
            }
        // } else {
            // std::cout << "pose_center skip " << ir << std::endl;
        }
    }
    cen /= double(count);
    // ::scheme::util::SimpleArray<3,float> center;
    Eigen::Vector3f center;
    center[0] = cen[0];
    center[1] = cen[1];
    center[2] = cen[2];
    return center;
}

void
get_rg_radius(
    core::pose::Pose const & pose,
    float & rg,
    float & radius,
    utility::vector1<core::Size> const & useres /*= utility::vector1<core::Size>()*/,
    bool allatom /*= false*/
){
    Eigen::Vector3f centmp = pose_center( pose, useres );
    numeric::xyzVector<double> cen;
    float maxdis = -9e9, avgdis2 = 0.0;
    for( int i = 0; i < 3; ++i ) cen[i] = centmp[i];
    for( int iri = 1; iri <= useres.size(); ++iri ){
        int ir = useres[iri];
        if( allatom ){
            for( int ia = 1; ia <= pose.residue(ir).nheavyatoms(); ++ia ){
                numeric::xyzVector<double> coord = pose.residue(ir).xyz(ia);
                avgdis2 += cen.distance_squared( coord );
                maxdis = std::max( maxdis, (float)cen.distance( coord ) );
            }
        } else {
            numeric::xyzVector<double> coord;
            if(      pose.residue(ir).has("CB") ) coord = pose.residue(ir).xyz("CB");
            else if( pose.residue(ir).has("CA") ) coord = pose.residue(ir).xyz("CA");
            else                                  coord = pose.residue(ir).nbr_atom_xyz();
            avgdis2 += cen.distance_squared( coord );
            maxdis = std::max( maxdis, (float)cen.distance( coord ) );
        }
    }
    avgdis2 /= useres.size();
    rg = sqrt( avgdis2 );
    radius = maxdis;
}

// in radians
float
angle_between_vectors(
	Eigen::Vector3f const & vec1,
	Eigen::Vector3f const & vec2
) {
	const float dot = vec1.dot(vec2);
	const float norm_product = vec1.norm() * vec2.norm();
	return std::acos( dot / norm_product );
}

void 
xform_pose( 
	core::pose::Pose & pose, 
	numeric::xyzTransform<float> s, 
	core::Size sres/*=1*/, 
	core::Size eres/*=0*/
) {
  if(eres==0) eres = pose.size();
  for(core::Size ir = sres; ir <= eres; ++ir) {
    for(core::Size ia = 1; ia <= pose.residue_type(ir).natoms(); ++ia) {
      core::id::AtomID const aid(core::id::AtomID(ia,ir));
      pose.set_xyz( aid, s*pose.xyz(aid) );
    }
  }
}

#ifdef USEGRIDSCORE
shared_ptr<protocols::ligand_docking::ga_ligand_dock::GridScorer>
prepare_grid_scorer(
	core::pose::Pose const & target,
	utility::vector1<core::Size> const & target_res,
	std::string const & atype_aas /*= "ACDEFGHIKLMNPQRSTVWY" */
) {

	using ObjexxFCL::format::F;

	std::vector<std::pair<core::Real,core::Real>> xyz_min_max(3);
	for ( std::pair<core::Real,core::Real> & pair : xyz_min_max ) { pair.first = 9e9; pair.second = -9e9; }

	for ( core::Size ir : target_res ) {
		for( int ia = 1; ia <= target.residue(ir).nheavyatoms(); ++ia ){
			numeric::xyzVector<core::Real> const & xyz = target.residue(ir).xyz(ia);
			
			for ( int j = 0; j < 3; j++ ) {
				xyz_min_max[j].first = std::min(xyz_min_max[j].first, xyz[j]);
				xyz_min_max[j].second = std::max(xyz_min_max[j].second, xyz[j]);
			}
		}
	}

	numeric::xyzVector< core::Real > centers(3);
	numeric::xyzVector< core::Real > radii(3);

	for ( int j = 0; j < 3; j++ ) {
		centers[j] = ( xyz_min_max[j].first + xyz_min_max[j].second ) / 2.0;
		radii[j] = ( xyz_min_max[j].second - centers[j] );
		runtime_assert( radii[j] > 0 );
	}


	std::cout << "Grid bounds: ";
	for ( int j = 0; j < 3; j++ ) {
		if (j > 0) std::cout << " x ";
		std::cout << "( " << F(5,1,xyz_min_max[j].first) << ", " << F(5,1,xyz_min_max[j].second) << " )";
	}
	std::cout << std::endl;

	radii += 5.0;	// max interaction cutoff

	std::cout << "Padded grid dimensions: ";
	for ( int j = 0; j < 3; j++ ) {
		if (j > 0) std::cout << " x ";
		std::cout << F(5,1,radii[j]); 
	}
	std::cout << std::endl;

	std::cout << "Grid volume: " << F(7,1,radii[0]*radii[1]*radii[2]*8) << std::endl;

	core::scoring::ScoreFunctionOP scorefxn = core::scoring::ScoreFunctionFactory::create_score_function("beta_nov16");
	shared_ptr<protocols::ligand_docking::ga_ligand_dock::GridScorer> grid_scorer 
		= make_shared<protocols::ligand_docking::ga_ligand_dock::GridScorer>( scorefxn );
	grid_scorer->set_grid_com_radii( centers, radii );

	// first A is the "ligand"
	std::string atype_aas_wlig = "A" + atype_aas;
	numeric::xyzVector< core::Real> far_away( 10000, 10000, 10000);
	numeric::xyzMatrix< core::Real> identity = numeric::xyzMatrix< core::Real>::identity();

	std::cout << "Using residue types: " << atype_aas << " for atom types" << std::endl;

	core::pose::PoseOP grid_pose = target.clone();
	utility::vector1< core::Size > fake_movingSCs;
	int ligand_resid = grid_pose->size() + 1;
	for ( int i = 0; i < atype_aas_wlig.size(); i++ ) {
		core::conformation::ResidueOP res = core::conformation::get_residue_from_name1( atype_aas_wlig[i] );
		res->apply_transform_Rx_plus_v( identity, far_away );
		grid_pose->append_residue_by_jump( *res, 1 );
		if ( i > 0 ) fake_movingSCs.push_back( grid_pose->size() );
	}

	grid_scorer->calculate_grid( *grid_pose, ligand_resid, fake_movingSCs );

	return grid_scorer;

}

#endif



}
}


