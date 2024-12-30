from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy, rmdir
import os

class StdxConan(ConanFile):
    name = "stdx"
    version = "0.1"
    license = "MIT"  # Replace with your license
    author = "Yashar Abbasalizadeh Rezaei"
    url = "https://github.com/yrezaei/stdx"
    description = "A modular C++ library"
    topics = ("c++", "library", "modular")
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_flag_module": [True, False],
        "build_dummy_module": [True, False]
    }
    default_options = {
        "shared": True,
        "fPIC": True,
        "build_flag_module": True,
        "build_dummy_module": True
    }
    exports_sources = (
        "CMakeLists.txt",
        "include/*",
        "src/*",
        "flag/*",
        "dummy/*"
    )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["BUILD_FLAG_MODULE"] = "ON" if self.options.build_flag_module else "OFF"
        tc.variables["BUILD_DUMMY_MODULE"] = "ON" if self.options.build_dummy_module else "OFF"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        # Copy headers and compiled libraries
        copy(self, "*.hpp", dst=os.path.join(self.package_folder, "include"), src=os.path.join(self.source_folder, "include"))
        copy(self, "*.a", dst="lib", src=self.build_folder, keep_path=False)
        copy(self, "*.lib", dst="lib", src=self.build_folder, keep_path=False)
        copy(self, "*.so", dst="lib", src=self.build_folder, keep_path=False)
        copy(self, "*.dll", dst="bin", src=self.build_folder, keep_path=False)
        copy(self, "*.dylib", dst="lib", src=self.build_folder, keep_path=False)


    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "stdx")
        self.cpp_info.set_property("cmake_target_name", "stdx::stdx")
        self.cpp_info.includedirs = ["include"]  # Specify the include directory

