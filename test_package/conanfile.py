from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain

class StdxTestPackageConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeDeps(self)
        tc.generate()
        toolchain = CMakeToolchain(self)
        toolchain.generate()

    def requirements(self):
        self.requires(self.tested_reference_str)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if not self.settings.os == "Windows" or self.settings.build_type == "Debug":
            self.run("test_package", cwd=self.build_folder)

