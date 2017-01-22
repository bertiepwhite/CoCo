#include <stdio.h>

#include <math.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <vector>

#include "vmath/loadtxt.hpp"
#include "vmath/convert.hpp"
#include "vmath/algebra.hpp"
#include "vmath/stat.hpp"

#include "core/utils.hpp"
#include "core/Cosmology.hpp"
#include "core/Filters.hpp"
#include "core/SN.hpp"
#include "solvers/MNest.hpp"
#include "models/SpecMangle.hpp"


// Data structure for parameters that are passed between functions
struct Workspace {
    // User inputs
    std::string inputFileName_;
    std::vector<std::string> specFileList_;
    std::vector<std::string> snNameList_;
    std::vector<double> mjdList_;
    std::vector<double> zList_;

    // Hash table of SN light curves
    std::unordered_map<std::string, SN> sn_;

    // Other helper classes
    std::string filterPath_;
    std::shared_ptr<Cosmology> cosmology_;
    std::shared_ptr<Filters> filters_;
};


void help() {
    std::cout << "CoCo - SpecFit: \n";
    std::cout << "Originally developed by Natasha Karpenka, ";
    std::cout << "and reimplemented by Szymon Prajs (S.Prajs@soton.ac.uk).\n";
    std::cout << "Currently maintained by Szymon Prajs and Rob Firth.\n";
    std::cout << "\nUsage:\n";
    std::cout << "./specfit spectra_setup_file.list\n";
    std::cout << "or\n";
    std::cout << "./specfit spectrum_file.* SN_name MJD rsedshift\n\n";
    std::cout << " spectra_setup_file.list must have the following columns:\n";
    std::cout << "Spectrum_file_path SN_name MJD redshift\n";
    std::cout << std::endl;
}


/* Assign input options to workspace parameters */
void applyOptions(std::vector<std::string> &options, std::shared_ptr<Workspace> w) {
    if (options.size() < 1 || options[0] == "-h" || options[0] == "--help") {
        help();
        exit(0);
    }

    // First option is the LC file name or list of LC files
    double skipOptions;
    w->inputFileName_ = options[0];
    if (options[0].substr(options[0].find_last_of(".") + 1) == "list") {
        std::vector< std::vector<std::string> > infoList =
            vmath::loadtxt<std::string>(w->inputFileName_, 4);
        w->specFileList_ = infoList[0];
        w->snNameList_ = infoList[1];
        w->mjdList_ = vmath::castString<double>(infoList[2]);
        w->zList_ = vmath::castString<double>(infoList[3]);
        skipOptions = 1;

    } else if (options.size() >= 4) {
        w->specFileList_ = {options[0]};
        w->snNameList_ = {options[1]};
        w->mjdList_ = {atof(options[2].c_str())};
        w->zList_ = {atof(options[3].c_str())};
        skipOptions = 3;

    } else {
        std::cout << "You need to provide either a *.list or 4 parameters" << std::endl;
        exit(0);
    }

    // Go though each option and assign the correct properties
    std::vector<std::string> command;
    for (size_t i = skipOptions; i < options.size(); ++i) {
        // Deal with flags by loading pairs of options into commands
        if (options[i] == "-f") {
            if (i+1 < options.size()) {
                command = {options[i], options[i+1]};
                i++;  // skip the next option as it's already assigned above

            } else {
                std::cout << options[i] << " is not a valid flag" << std::endl;
            }

        } else if (options[i] == "-h" || options[i] == "--help"){
            help();
            continue;

        } else {
            utils::split(options[i], '=', command);
        }

        // Assign properties based on commands
        if (command.size() != 2) {
            std::cout << command[0] << " is not a valid command." << std::endl;
            continue;

        } else {
            std::cout << command[0] << " is not a valid command." << std::endl;
        }
    }
}


/* Automatically fill in all unassigned properties with defaults */
void fillUnassigned(std::shared_ptr<Workspace> w) {
    // Do a sanity check for the LC files
    if (w->specFileList_.size() == 0) {
        std::cout << "Something went seriously wrong. ";
        std::cout << "Please consider report this bug on our project GitHub page";
        std::cout << std::endl;
        exit(0);
    }

	// Load each spectrum into the correct SN object
    for (size_t i = 0; i < w->specFileList_.size(); ++i) {
        // Load the light curve if not yet loaded
        if (w->sn_.find(w->snNameList_[i]) == w->sn_.end()) {
            if (!utils::fileExists("recon/" + w->snNameList_[i] + ".dat")) {
                std::cout << "No reconstructed light curve was found for: ";
                std::cout << w->snNameList_[i] << "\nExiting!" << std::endl;
                exit(0);
            }

            w->sn_[w->snNameList_[i]] = SN("recon/" + w->snNameList_[i] + ".dat");
            w->sn_[w->snNameList_[i]].z_ = w->zList_[i];
        }

        // Load each stectrum into the correct SN object
        if (!utils::fileExists(w->specFileList_[i])) {
            std::cout << "Ignoring spectrum - path not found: ";
            std::cout << w->specFileList_[i] << std::endl;
            exit(0);
        } else {
            w->sn_[w->snNameList_[i]].addSpec(w->specFileList_[i], w->mjdList_[i]);
        }
    }
}


void mangleSpectra(std::shared_ptr<Workspace> w) {
    // Loop though each SN
    for (auto sn : w->sn_) {
        // Loop though each spectrum
        for (auto &spec : sn.second.spec_) {
            // Initialise the model
            std::shared_ptr<SpecMangle> specMangle(new SpecMangle);
            specMangle->lcData_ = sn.second.epoch_[spec.second.mjd_];
            specMangle->specData_ = spec.second;

            // Normalise the spectrum before fitting
            specMangle->specData_.flux_ =
                vmath::div<double>(specMangle->specData_.flux_, spec.second.fluxNorm_);

            // Rescale filters to the data wavelength and assign to model
            w->filters_->rescale(spec.second.wav_);
            specMangle->filters_ = w->filters_;

            // Assign filter central wavelengths to each lc data point
            for (auto &obs : specMangle->lcData_) {
                obs.wav_ = w->filters_->filter_[obs.filter_].centralWavelength_;
            }

            // Sort light curve slice by filter central wavelengths
            std::sort(specMangle->lcData_.begin(), specMangle->lcData_.end(),
                      [](const Obs &a, const Obs &b) -> bool {
                         return a.wav_ < b.wav_;
                      });

            // Set priors and number of paramters
            specMangle->setPriors();

            // Initialise the solver
            std::shared_ptr<Model> model = dynamic_pointer_cast<Model>(specMangle);
            std::shared_ptr<MNest> mnest(new MNest(model));
            mnest->livePoints_ = 10;

            std::shared_ptr<Solver> solver = dynamic_pointer_cast<Solver>(mnest);
            solver->xRecon_ = spec.second.wav_;
            solver->chainPath_ = "chains/" + sn.second.name_ + "/" + to_string(spec.second.mjd_);

            // Perform fitting
            solver->analyse();

            // Reset spectrum units to original
            solver->bestFit_ = vmath::mult<double>(solver->bestFit_, spec.second.fluxNorm_);
            solver->mean_ = vmath::mult<double>(solver->mean_, spec.second.fluxNorm_);
            solver->meanSigma_ = vmath::mult<double>(solver->meanSigma_, spec.second.fluxNorm_);
            solver->median_ = vmath::mult<double>(solver->median_, spec.second.fluxNorm_);
            solver->medianSigma_ = vmath::mult<double>(solver->medianSigma_, spec.second.fluxNorm_);

            // File handels for spectrum mangling results
            ofstream reconSpecFile;
            ofstream reconStatFile;
            reconSpecFile.open("recon/" + sn.second.name_ + "_" +
                               to_string(spec.second.mjd_) + ".spec");
            reconStatFile.open("recon/" + sn.second.name_ + "_" +
                               to_string(spec.second.mjd_) + ".stat");

            // Write reconstructed spectra to a file
            for (size_t i = 0; i < solver->xRecon_.size(); ++i) {
                reconSpecFile << solver->xRecon_[i] << " " << solver->mean_[i];
                reconSpecFile << " " << solver->meanSigma_[i] << " " << "\n";

                reconStatFile << solver->xRecon_[i] << " " << solver->mean_[i] << " ";
                reconStatFile << solver->meanSigma_[i] << " " << solver->bestFit_[i] << " ";
                reconStatFile << solver->median_[i] << " " << solver->medianSigma_[i] << "\n";
            }

            reconSpecFile.close();
            reconStatFile.close();
        }
    }
}


int main(int argc, char *argv[]) {
    std::vector<std::string> options;
    std::shared_ptr<Workspace> w(new Workspace());
    w->cosmology_ = std::shared_ptr<Cosmology>(new Cosmology());

    utils::getArgv(argc, argv, options);
    applyOptions(options, w);
    fillUnassigned(w);

    // Load the filter responses
    w->filterPath_ = "data/filters";
    w->filters_ = std::shared_ptr<Filters>(new Filters(w->filterPath_));

    mangleSpectra(w);

    return 0;
}
