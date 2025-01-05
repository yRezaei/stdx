from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain

class TestPackageConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        # Require the package being tested
        self.requires(self.tested_reference_str)

    def generate(self):
        tc = CMakeToolchain(self)

        # Add preprocessor definitions for enabled modules
        tc.variables["STDX_ENABLE_FLAG"] = "ON" if self.dependencies["stdx"].options.enable_flag else "OFF"
        tc.variables["STDX_ENABLE_LOGGER"] = "ON" if self.dependencies["stdx"].options.enable_logger else "OFF"

        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if not self.conf.get("tools.build:skip_test"):
            # Run logger test if enabled
            if self.dependencies["stdx"].options.enable_logger:
                self.run("./test_logger", env="conanrun")
            # Run flag test if enabled
            if self.dependencies["stdx"].options.enable_flag:
                self.run("./test_flag", env="conanrun")
