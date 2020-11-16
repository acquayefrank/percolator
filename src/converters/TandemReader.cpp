// The file specification of the X!Tandem output is given here:
//   https://www.thegpm.org/docs/X_series_output_form.pdf

#include "TandemReader.h"

//default score vector //TODO move this to a file or input parameter                          
const std::map<string,double> TandemReader::tandemFeaturesDefaultValue =
  boost::assign::map_list_of("hyperscore", 0.8)
    ("deltaScore", 1.9)
    ("frac_ion_b", 0.0)
    ("frac_ion_y", 0.0)
    ("Mass", 0.0)
    ("dM", 0.0)
    ("absdM", -0.03)
    ("PepLen", 0.0)
    ("Charge2", 0.0)
    ("Charge3", 0.0)
    ("enzN", 0.0)
    ("enzC", 0.0)
    ("enzInt", 0.0);

static const XMLCh groupStr[] = { chLatin_g, chLatin_r, chLatin_o, chLatin_u, chLatin_p, chNull};
static const XMLCh groupTypeStr[] = { chLatin_t, chLatin_y, chLatin_p, chLatin_e, chNull};
static const XMLCh groupModelStr[] = { chLatin_m, chLatin_o, chLatin_d, chLatin_e, chLatin_l, chNull};
static const std::string schemaDefinition = Globals::getInstance()->getXMLDir(true)+TANDEM_SCHEMA_LOCATION + string("tandem2011.12.01.1.xsd");
static const std::string scheme_namespace = TANDEM_NAMESPACE;
static const std::string schema_major = boost::lexical_cast<string>(TANDEM_VERSION);
static const std::string schema_minor = boost::lexical_cast<string>(TANDEM_VERSION);

TandemReader::TandemReader(ParseOptions po):Reader(po) {
  x_score = false;
  y_score = false;
  z_score = false;
  a_score = false;
  b_score = false;
  c_score = false;
  firstPSM = true;
}

TandemReader::~TandemReader() {}


//NOTE Function to add namespace to a dom documents all elements except elements that has the namespace www.bioml.com/gaml/

//Checks validity of the file and also if the defaultNameSpace is declared or not.
bool TandemReader::checkValidity(const std::string &file) {
  bool isvalid;
  std::ifstream fileIn(file.c_str(), std::ios::in);
  if (!fileIn) {
    ostringstream temp;
    temp << "Error : can not open file " << file << std::endl;
    throw MyException(temp.str());
  }
  std::string line,line_xml,line_bioml;
  if (!getline(fileIn, line)) {
    ostringstream temp;
    temp << "Error: cannot read file " << file << std::endl;
    throw MyException(temp.str());
  }
  
  if (line.find("<?xml") != std::string::npos) {
    getline(fileIn, line_xml);
    getline(fileIn, line_bioml);
    
    if (line_bioml.find("<bioml") == std::string::npos) {
      std::cerr << "Warning: XML file not generated by X!tandem, input file should be in BIOML format: " << file << std::endl;
      isvalid = false;
    } else {
      isvalid = true;
    }
  } else {
    isvalid = false;
  }

  fileIn.close();
  return isvalid;
}

bool TandemReader::checkIsMeta(const std::string &file) {
  bool ismeta;
  std::string line;
  std::ifstream fileIn(file.c_str(), std::ios::in);
  getline(fileIn, line);
  
  if (line.find("<?xml") != std::string::npos) {
    ismeta = false;
  } else {
    ismeta = true;
  }

  fileIn.close();
  return ismeta;
}


void TandemReader::addFeatureDescriptions(bool doEnzyme) {
  push_backFeatureDescription("hyperscore","",tandemFeaturesDefaultValue.at("hyperscore"));
  push_backFeatureDescription("deltaScore","",tandemFeaturesDefaultValue.at("deltaScore"));  //hypercore - abs(nextscore);
  
  /* THESE FEATURES ARE NOT VALIDATED
  push_backFeatureDescription("DomainExpectedValue");
  push_backFeatureDescription("LogDomainExpectedValue");
  push_backFeatureDescription("SpectraSumIon");
  push_backFeatureDescription("SpectraMaxIon");
  push_backFeatureDescription("Missed_cleaveges");
  */
  
  if (a_score) {
    //push_backFeatureDescription("score_ion_a");
    push_backFeatureDescription("frac_ion_a");
  }
  if (b_score) {
    //push_backFeatureDescription("score_ion_b");
    push_backFeatureDescription("frac_ion_b","",tandemFeaturesDefaultValue.at("frac_ion_b"));
  }
  if (c_score) {
    //push_backFeatureDescription("score_ion_c");
    push_backFeatureDescription("frac_ion_c");
  }
  
  if (x_score) {
    //push_backFeatureDescription("score_ion_x");
    push_backFeatureDescription("frac_ion_x");
  }
  if (y_score) {
    //push_backFeatureDescription("score_ion_y");
    push_backFeatureDescription("frac_ion_y","",tandemFeaturesDefaultValue.at("frac_ion_y"));
  }
  if (z_score) {
    //push_backFeatureDescription("score_ion_z");
    push_backFeatureDescription("frac_ion_z");
  }
  
  push_backFeatureDescription("Mass","",tandemFeaturesDefaultValue.at("Mass"));
  //Mass difference
  push_backFeatureDescription("dM","",tandemFeaturesDefaultValue.at("dM"));
  push_backFeatureDescription("absdM","",tandemFeaturesDefaultValue.at("absdM"));
  push_backFeatureDescription("PepLen","",tandemFeaturesDefaultValue.at("PepLen"));
  
  for (int charge = minCharge; charge <= maxCharge; ++charge) {
    std::ostringstream cname;
    cname << "Charge" << charge;
    push_backFeatureDescription(cname.str().c_str());

  }
  if (doEnzyme) {
    push_backFeatureDescription("enzN","",tandemFeaturesDefaultValue.at("enzN"));
    push_backFeatureDescription("enzC","",tandemFeaturesDefaultValue.at("enzC"));
    push_backFeatureDescription("enzInt","",tandemFeaturesDefaultValue.at("enzInt"));
  }
  
  if (po.calcPTMs) {
    push_backFeatureDescription("ptm");
  }
  
  if (po.pngasef) {
    push_backFeatureDescription("PNGaseF");
  }
  
  if (po.calcAAFrequencies) {
    BOOST_FOREACH (const char aa, freqAA) {
      std::string temp = std::string(1,aa) + "-Freq";
      push_backFeatureDescription(temp.c_str());
    }
  }
}

//Get max and min chage but also check what type of a,b,c,x,y,z score/ions are present
void TandemReader::getMaxMinCharge(const std::string &fn, bool isDecoy){
  
  int nTot = 0, charge = 0;
  
  namespace xml = xsd::cxx::xml;
 
  ifstream ifs;
  ifs.exceptions(ifstream::badbit|ifstream::failbit);
    
  ifs.open(fn.c_str());
  if (!ifs) {
    ostringstream temp;
    temp << "Error : can not open file " << fn << std::endl;
    throw MyException(temp.str());
  }
  parser p;
  
  try {
    bool validateSchema = true;
    xml_schema::dom::auto_ptr< xercesc::DOMDocument> 
    doc (p.start (ifs, fn.c_str(), validateSchema, schemaDefinition, schema_major, schema_minor, scheme_namespace,true));
    assert(doc.get());
    
    for (doc = p.next(); doc.get() != 0; doc = p.next ()) {  
      //Check that the tag name is group and that its not the inputput parameters
      if (XMLString::equals(groupStr,doc->getDocumentElement()->getTagName()) 
        	&& XMLString::equals(groupModelStr,doc->getDocumentElement()->getAttribute(groupTypeStr))) {
	      tandem_ns::group groupObj(*doc->getDocumentElement()); //Parse to the codesynthesis object model
	      
	      //We are sure we are not in parameters group so z(the charge) has to be present.
	      if (groupObj.z().present()) {
	        stringstream chargeStream (stringstream::in | stringstream::out);
	        chargeStream << groupObj.z();
	        chargeStream >> charge;
	        if (minCharge > charge) minCharge = charge;
	        if (maxCharge < charge) maxCharge = charge;
	        nTot++;
	      } else {
	        ostringstream temp;
	        temp << "Missing charge(attribute z in group element) for one or more groups in: " << fn << endl;
	        throw MyException(temp.str());
	      }
	      if (firstPSM) {
	        //Check what type of scores/ions are present
	        BOOST_FOREACH (const tandem_ns::protein &protObj, groupObj.protein()) { //Protein
	          tandem_ns::protein::peptide_type peptideObj=protObj.peptide(); //Peptide
	          BOOST_FOREACH(const tandem_ns::domain &domainObj, peptideObj.domain()) { //Domain
	            //x,y,z
	            if (domainObj.x_score().present() && domainObj.x_ions().present()) {
            		x_score=true;
	            }
	            if (domainObj.y_score().present() && domainObj.y_ions().present()) {
            		y_score=true;
	            }
	            if (domainObj.z_score().present() && domainObj.z_ions().present()) {
		            z_score=true;
	            }
	            //a,b,c
	            if (domainObj.a_score().present() && domainObj.a_ions().present()) {
            		a_score=true;
	            }
	            if (domainObj.b_score().present() && domainObj.b_ions().present()) {
            		b_score=true;
	            }
	            if (domainObj.c_score().present() && domainObj.c_ions().present()) {
            		c_score=true;
	            }
	          } //End of for domain
	        } //End of for prot
	        firstPSM=false;
	      }
      }
    }
  } catch (const xml_schema::exception& e) {
    ifs.close();
    ostringstream temp;
    temp << "ERROR parsing the xml file: " << fn << endl;
    temp << e << endl;
    throw MyException(temp.str());
  } catch (MyException e) {
	  std::cerr << "Error reading file: " << fn << "\n  " << e.what() << std::endl;
	  exit(1);
  } catch (std::exception e) {
    std::cerr << "Error reading file: " << fn << "\n  Unknown exception in getMaxMinCharge: " << e.what() << std::endl;
  }
  
  ifs.close();
  if (nTot<=0) {
    ostringstream temp;
    temp << "The file " << fn << " does not contain any records" << std::endl;
    throw MyException(temp.str());
  }
  
  return;
}

//Get the groupObject which contains one spectra but might contain several psms. 
//All psms are read, features calculated and the psm saved.
void TandemReader::readSpectra(const tandem_ns::group &groupObj, bool isDecoy,
    boost::shared_ptr<FragSpectrumScanDatabase> database, const std::string &fn) {
  std::string fileId, proteinName;
  int rank = 1, spectraId;
  double parentIonMass = 0.0;
  unsigned charge = 0;
  double sumI = 0.0;
  double maxI = 0.0;
  //double fI = 0.0;
  spectraId = boost::lexical_cast<int>(groupObj.id());
  peptideProteinMapType peptideProteinMap;
  getPeptideProteinMap(groupObj, peptideProteinMap);
  
  if (groupObj.mh().present() && groupObj.z().present() && groupObj.sumI().present() && 
    groupObj.maxI().present() && groupObj.fI().present()&& groupObj.id().present()) {
    parentIonMass = boost::lexical_cast<double>(groupObj.mh().get());//the parent ion mass (plus a proton) from the spectrum
    charge = boost::lexical_cast<unsigned>(groupObj.z().get()); 	//the parent ion charge from the spectrum
    sumI = boost::lexical_cast<double>(groupObj.sumI().get());	//the log10 value of the sum of all of the fragment ion intensities
    maxI = boost::lexical_cast<double>(groupObj.maxI().get());	//the maximum fragment ion intensity
  } else {
    ostringstream temp;
    temp << "Error : A required attribute is not present in the group/spectra element in file: " << fn << endl;
    throw MyException(temp.str());
  }
  std::set<std::string> seenPeptides;
  //Loop through the protein objects
  BOOST_FOREACH(const tandem_ns::protein &protObj, groupObj.protein()) {
    proteinName = getRidOfUnprintables(protObj.label());

    tandem_ns::peptide peptideObj = protObj.peptide();
    for (tandem_ns::peptide::domain_iterator iter = peptideObj.domain().begin();
      	  iter != peptideObj.domain().end(); ++iter) {
      tandem_ns::peptide::domain_type domain = *iter;
      std::string peptide = boost::lexical_cast<std::string>(domain.seq());
      if (rank <= po.hitsPerSpectrum && seenPeptides.find(peptide) == seenPeptides.end()) {
        seenPeptides.insert(peptide);
	      fileId = fn;
	      size_t spos = fileId.rfind('/');
	      if (spos != std::string::npos) fileId.erase(0, spos + 1);
	      spos = fileId.find('.');
	      if (spos != std::string::npos) fileId.erase(spos);
	      //Create id
	      std::string psmId = createPsmId(fileId, parentIonMass, spectraId, charge, rank);
      	createPSM(domain, parentIonMass, charge, sumI, maxI, isDecoy, database, peptideProteinMap, psmId, spectraId);
      	++rank;
      }//End of if rank<=po.hitsPerSpectrum
    }
  } //End of boost protein
}

//Loops through the spectra(group object) and makes a map of peptides with a set of proteins as value
void TandemReader::getPeptideProteinMap(const tandem_ns::group &groupObj,
    peptideProteinMapType &peptideProteinMap) {
  BOOST_FOREACH(const tandem_ns::protein &protObj, groupObj.protein()) {
    std::string proteinName = getRidOfUnprintables(protObj.label());
    tandem_ns::peptide peptideObj = protObj.peptide();
    
    for (tandem_ns::peptide::domain_iterator iter = peptideObj.domain().begin();
    	    iter != peptideObj.domain().end(); ++iter) {
      std::string peptide = (*iter).seq();
      peptideProteinMap[peptide].insert(proteinName);
    }
  }
}

//Calculates some features then creates the psm and saves it
void TandemReader::createPSM(const tandem_ns::peptide::domain_type &domain,
    double parentIonMass, unsigned charge, double sumI, double maxI, 
    bool isDecoy, boost::shared_ptr<FragSpectrumScanDatabase> database,
    const peptideProteinMapType &peptideProteinMap,const string &psmId, 
    int spectraId) {
  std::map<char,int> ptmMap = po.ptmScheme;
  std::auto_ptr< percolatorInNs::features >  features_p( new percolatorInNs::features ());
  percolatorInNs::features::feature_sequence & f_seq =  features_p->feature();
  double calculated_mass = boost::lexical_cast<double>(domain.mh());
  double mass_diff = boost::lexical_cast<double>(domain.delta());
  double hyperscore = boost::lexical_cast<double>(domain.hyperscore());
  double next_hyperscore = boost::lexical_cast<double>(domain.nextscore());
  std::string peptide = boost::lexical_cast<std::string>(domain.seq());
  
  std::set<std::string> proteinOccuranceSet = peptideProteinMap.at(peptide);
  assert(proteinOccuranceSet.size() > 0);
  std::vector<std::string> proteinOccurences;
  set<std::string>::iterator posIt;
  if (po.iscombined) isDecoy = true; // Adjust isDecoy if combined file
  for (posIt = proteinOccuranceSet.begin(); posIt != proteinOccuranceSet.end(); ++posIt) {
    if (po.iscombined && isDecoy) {
  	  isDecoy = posIt->find(po.reversedFeaturePattern, 0) != std::string::npos;
    }
    proteinOccurences.push_back(*posIt);
  }
  
  std::string pre = boost::lexical_cast<std::string>(domain.pre());
  if (pre=="[") {
	  pre = "-";
  }
  std::string flankN = boost::lexical_cast<std::string>(pre.at(pre.size()-1));
  std::string post = boost::lexical_cast<std::string>(domain.post());
  if (post=="]") {
	 post = "-";
  }
  std::string flankC = boost::lexical_cast<std::string>(post.at(0));
  std::string fullpeptide = flankN + "." + peptide + "." + flankC;
  double xions = 0.0,yions = 0.0,zions = 0.0,aions = 0.0,bions = 0.0,cions = 0.0;
  if (x_score) {
    //xscore = boost::lexical_cast<double>(domain.x_score());
    xions = boost::lexical_cast<double>(domain.x_ions());
  }
  if (y_score) {
    //yscore = boost::lexical_cast<double>(domain.y_score()); 
    yions = boost::lexical_cast<double>(domain.y_ions());
  }
  if (z_score) {
    //zscore = boost::lexical_cast<double>(domain.z_score());
    zions = boost::lexical_cast<double>(domain.z_ions());
  }
  if (a_score) {
    //ascore = boost::lexical_cast<double>(domain.a_score());
    aions = boost::lexical_cast<double>(domain.a_ions());
  }
  if (b_score) {
    //bscore = boost::lexical_cast<double>(domain.b_score());
    bions = boost::lexical_cast<double>(domain.b_ions());
  }
  if (c_score) {
    //cscore = boost::lexical_cast<double>(domain.c_score());
    cions = boost::lexical_cast<double>(domain.c_ions());
  }
  
  //Remove modifications
  std::string peptideS = peptide; // save modified peptide for later
  for(unsigned int ix=0;ix<peptide.size();++ix) {
    if (freqAA.find(peptide[ix]) == string::npos) {
      if (ptmMap.count(peptide[ix])==0) {
	      ostringstream temp;
	      temp << "Error : Peptide sequence " << peptide
	           << " contains modification " << peptide[ix] << " that is not specified by a \"-p\" argument" << endl;
	      throw MyException(temp.str());
      }
      peptide.erase(ix--,1);
    }  
  }

  std::auto_ptr< percolatorInNs::peptideType > peptide_p( new percolatorInNs::peptideType(peptide) );
  
  // Register the ptms (modifications)
  for(unsigned int ix=0;ix<peptideS.size();++ix) {
    if (freqAA.find(peptideS[ix]) == string::npos) {
      int accession = ptmMap[peptideS[ix]];
      std::auto_ptr< percolatorInNs::uniMod > um_p(new percolatorInNs::uniMod(accession));
      std::auto_ptr< percolatorInNs::modificationType > mod_p( new percolatorInNs::modificationType(ix));
      mod_p->uniMod(um_p);
      peptide_p->modification().push_back(mod_p);      
      peptideS.erase(ix--,1);
    }  
  }
  
  // Register more ptms
  if (domain.aa().size() > 0) {
    int peptideInProtStartPos = boost::lexical_cast<int>(domain.start());
    int peptideInProtEndPos = boost::lexical_cast<int>(domain.end());
    BOOST_FOREACH(const tandem_ns::aa &aaObj, domain.aa()) {
      int modPos = boost::lexical_cast<int>(aaObj.at());
      if (modPos < peptideInProtStartPos || modPos > peptideInProtEndPos) {
        ostringstream temp;
        temp << "Error: Peptide sequence " << peptide
             << " contains modification [" << aaObj.modified() << "] at protein position " 
             << modPos << ", which is outside of the peptide interval [" 
             << peptideInProtStartPos << "," << peptideInProtEndPos << "]." << endl;
        throw MyException(temp.str());
      }
      int relativeModPos = modPos - peptideInProtStartPos + 1;
      // aaObj.type(); // gives the amino acid that was modified. Redundant information as we have the position already, could be used for assertion
      std::auto_ptr< percolatorInNs::modificationType >  mod_p( new percolatorInNs::modificationType(relativeModPos));
      std::string mod_acc = aaObj.modified(); // modification mass
      std::auto_ptr< percolatorInNs::freeMod > fm_p (new percolatorInNs::freeMod(mod_acc));
      mod_p->freeMod(fm_p);
      peptide_p->modification().push_back(mod_p);
    }
  }

  //Push back the main scores
  f_seq.push_back(hyperscore);
  f_seq.push_back(hyperscore - next_hyperscore);
  //ions fractions
  if (a_score) f_seq.push_back(aions / peptide.size());
  if (b_score) f_seq.push_back(bions / peptide.size());
  if (c_score) f_seq.push_back(cions / peptide.size());
  if (x_score) f_seq.push_back(xions / peptide.size());
  if (y_score) f_seq.push_back(yions / peptide.size());
  if (z_score) f_seq.push_back(zions / peptide.size());
  //Mass
  f_seq.push_back(parentIonMass);
  f_seq.push_back(mass_diff);
  f_seq.push_back(abs(mass_diff));
  //peptide length
  f_seq.push_back(peptideLength(fullpeptide));

  //Charge
  for (int c = minCharge; c <= maxCharge; c++) {
    f_seq.push_back( charge == c ? 1.0 : 0.0);
  }

  //Enzyme
  if (enzyme_->getEnzymeType() != Enzyme::NO_ENZYME) {
    std::string peptideNoMods = removePTMs(peptide, ptmMap);
    f_seq.push_back(enzyme_->isEnzymatic(peptideNoMods.at(0),peptideNoMods.at(2)) ? 1.0 : 0.0);
    f_seq.push_back(enzyme_->isEnzymatic(peptideNoMods.at(peptideNoMods.size() - 3),peptideNoMods.at(peptideNoMods.size() - 1)) ? 1.0 : 0.0);
    std::string peptide2 = peptideNoMods.substr(2, peptideNoMods.length() - 4);
    f_seq.push_back( (double)enzyme_->countEnzymatic(peptide2) );
  }

  //PTM
  if (po.calcPTMs) {
    f_seq.push_back(cntPTMs(fullpeptide, ptmMap));
  }
  //PNGA
  if (po.pngasef) {
    f_seq.push_back(isPngasef(fullpeptide,isDecoy));
  }
  //AA FREQ
  if (po.calcAAFrequencies) {
    computeAAFrequencies(fullpeptide, f_seq);
  }  
    
  //Save the psm
  std::auto_ptr< percolatorInNs::peptideSpectrumMatch > psm_p(
      new percolatorInNs::peptideSpectrumMatch(features_p, peptide_p, psmId, 
      isDecoy, parentIonMass, calculated_mass, charge));
  
  std::vector<std::string>::const_iterator poIt;
  for (poIt = proteinOccurences.begin(); poIt != proteinOccurences.end(); ++poIt) {
    std::auto_ptr< percolatorInNs::occurence > oc_p(new percolatorInNs::occurence(*poIt, flankN, flankC));
    psm_p->occurence().push_back(oc_p);
  }
  
  database->savePsm(spectraId, psm_p);
}

void TandemReader::read(const std::string &fn, bool isDecoy,
    boost::shared_ptr<FragSpectrumScanDatabase> database) {
  std::string line, tmp, prot;
  std::istringstream lineParse;
  std::ifstream tandemIn;
  
  namespace xml = xsd::cxx::xml;
  
  ifstream ifs;
  ifs.exceptions(ifstream::badbit|ifstream::failbit);
  ifs.open(fn.c_str());
  parser p;
  
  //Sending defaultNameSpace as the bool for validation since if its not fixed 
  //the namespace has to be added later and then we cant validate the schema and xml file.
  xml_schema::dom::auto_ptr< xercesc::DOMDocument> doc(p.start(ifs, 
      fn.c_str(), true, schemaDefinition, schema_major, schema_minor, 
      scheme_namespace, true));
  assert(doc.get());   
  
  //tandem_ns::bioml biomlObj=biomlObj(*doc->getDocumentElement()); 
  //NOTE the root of the element, doesn't have any useful attributes
  //Loops over the group elements which are the spectra and the last 3 are the input parameters
  for (doc = p.next(); doc.get() != 0; doc = p.next()) {
    //NOTE can't access mixed content using codesynthesis, need to keep dom association. See the manual for tree parser: 
    //http://www.codesynthesis.com/pipermail/xsd-users/2008-October/002005.html
    //Not implemented here
    //Check that the tag name is group and that its not the input parameters
    if (XMLString::equals(groupStr,doc->getDocumentElement()->getTagName()) 
          && XMLString::equals(groupModelStr,doc->getDocumentElement()->getAttribute(groupTypeStr))) {
      tandem_ns::group groupObj(*doc->getDocumentElement()); //Parse it the codesynthesis object model.
      readSpectra(groupObj,isDecoy,database,fn); //The groupObj contains the psms
    }
  }
  ifs.close();
}

