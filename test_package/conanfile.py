from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.build import can_run
import os

class TestPackageConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    def requirements(self):
        self.requires(self.tested_reference_str)
        
    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["STDX_ENABLE_FLAG"] = "ON" if self.dependencies["stdx"].options.enable_flag else "OFF"
        tc.variables["STDX_ENABLE_LOGGER"] = "ON" if self.dependencies["stdx"].options.enable_logger else "OFF"
        tc.generate()
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            if self.dependencies["stdx"].options.enable_logger:
                self.run(os.path.join(self.cpp.build.bindir, "test_logger"), env="conanrun")
            if self.dependencies["stdx"].options.enable_flag:
                self.run(os.path.join(self.cpp.build.bindir, "test_flag"), env="conanrun")
