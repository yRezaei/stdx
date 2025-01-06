from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
import os

class TestPackageConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_PREFIX_PATH"] = os.path.join(self.dependencies["stdx"].package_folder, "lib", "cmake", "stdx_logger").replace("\\", "/")
        tc.variables["STDX_ENABLE_FLAG"] = "ON" if self.dependencies["stdx"].options.enable_flag else "OFF"
        tc.variables["STDX_ENABLE_LOGGER"] = "ON" if self.dependencies["stdx"].options.enable_logger else "OFF"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if self.dependencies["stdx"].options.enable_logger:
            self.run("./test_logger", env="conanrun")
        if self.dependencies["stdx"].options.enable_flag:
            self.run("./test_flag", env="conanrun")
