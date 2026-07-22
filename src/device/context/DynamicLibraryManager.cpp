// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DynamicLibraryManager.hpp"
#include "DynamicLibraryHelper.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

#include <vector>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace libobsensor {

#if defined(__APPLE__) && TARGET_OS_IOS
// On iOS, bare dylibs cannot be embedded inside a framework, so extension libraries are packaged as
// standalone frameworks (e.g. "ob_priv_filter.framework/ob_priv_filter") and embedded in the app bundle
// next to the SDK framework. Try the framework layout before falling back to the plain dylib path.
static std::shared_ptr<dylib> tryLoadExtensionFramework(const std::string &moduleLoadPath, const std::string &libraryName) {
    const std::string frameworkRelPath = libraryName + ".framework/" + libraryName;

    std::vector<std::string> candidateDirs = { moduleLoadPath };
    // sibling frameworks directory of the current SDK binary (e.g. MyApp.app/Frameworks)
    const auto moduleDir = utils::getCurrentWorkDirectory();
    const auto slashPos  = moduleDir.find_last_of('/');
    if(slashPos != std::string::npos) {
        candidateDirs.push_back(moduleDir.substr(0, slashPos));
    }
    candidateDirs.emplace_back("@rpath");

    for(const auto &dir: candidateDirs) {
        if(dir.empty()) {
            continue;
        }
        try {
            return std::make_shared<dylib>(dir, frameworkRelPath, dylib::no_filename_decorations);
        }
        catch(const std::exception &e) {
            LOG_DEBUG("Failed to load extension framework {}/{}: {}", dir, frameworkRelPath, e.what());
        }
    }
    return nullptr;
}
#endif

std::mutex                           DynamicLibraryManager::instanceMutex_;
std::weak_ptr<DynamicLibraryManager> DynamicLibraryManager::instanceWeakPtr_;

std::shared_ptr<DynamicLibraryManager> DynamicLibraryManager::getInstance() {
    std::unique_lock<std::mutex> lock(instanceMutex_);
    auto                         instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<DynamicLibraryManager>(new DynamicLibraryManager());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

DynamicLibraryManager::DynamicLibraryManager() = default;

void DynamicLibraryManager::registerLoadedCallback(const std::string &libraryName, LibraryLoadedCallback callback) {
    std::unique_lock<std::recursive_mutex> lock(libraryMutex_);
    loadedCallbacks_[libraryName] = callback;
}

std::shared_ptr<dylib> DynamicLibraryManager::getLibrary(const std::string &libraryName) {
    std::unique_lock<std::recursive_mutex> lock(libraryMutex_);
    auto                                   libIt = libMap_.find(libraryName);
    return libIt != libMap_.end() ? libIt->second : nullptr;
}

std::shared_ptr<dylib> DynamicLibraryManager::loadLibrary(const std::string &moduleLoadPath, const std::string &libraryName) {
    std::unique_lock<std::recursive_mutex> lock(libraryMutex_);
    auto                                   libIt = libMap_.find(libraryName);
    if(libIt != libMap_.end()) {
        return libIt->second;
    }

    LOG_DEBUG("Current working directory: {}", utils::getCurrentWorkDirectory());

    std::shared_ptr<dylib> library;
#if defined(__APPLE__) && TARGET_OS_IOS
    library = tryLoadExtensionFramework(moduleLoadPath, libraryName);
#endif
    if(!library) {
        library = std::make_shared<dylib>(moduleLoadPath.c_str(), libraryName.c_str());
    }
    libMap_[libraryName] = library;

    auto callbackIt = loadedCallbacks_.find(libraryName);
    if(callbackIt != loadedCallbacks_.end()) {
        callbackIt->second(libraryName, library);
    }
    DynamicLibraryHelper::logExtensionsCommitHashes(libraryName, library);
    return library;
}

}  // namespace libobsensor
