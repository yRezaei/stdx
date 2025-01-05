from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps

class StdxConan(ConanFile):
    name = "stdx"
    version = "0.1"
    license = "MIT"
    author = "Yashar A.Rezaei"
    url = "https://github.com/yrezaei/stdx"
    description = "Collection of C++ modules"
    topics = ("logger", "flag", "utilities")

    # Only include the recipe in the Conan repository
    exports = "conanfile.py"
    
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        # Enable/disable modules
        "enable_flag": [True, False],
        "enable_logger": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "enable_flag": True,
        "enable_logger": True,
    }

    def configure(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        pass  # Layout can be configured if needed

    def source(self):
        # Fetch the full source code from your main GitHub repo
        self.run("git clone https://github.com/yrezaei/stdx.git")
        self.run("git checkout v0.1")  # Optional: Checkout specific version/tag

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_BUILD_TYPE"] = str(self.settings.build_type)

        # If user says -o stdx:shared=True, then set BUILD_SHARED_LIBS=ON
        tc.cache_variables["BUILD_SHARED_LIBS"] = "ON" if self.options.shared else "OFF"

        # Pass module enable flags to CMake
        tc.preprocessor_definitions["STDX_ENABLE_FLAG"] = \
            "ON" if self.options.enable_flag else "OFF"
        tc.preprocessor_definitions["STDX_ENABLE_LOGGER"] = \
            "ON" if self.options.enable_logger else "OFF"

        tc.generate()

        cd = CMakeDeps(self)
        cd.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.test()  # Optionally run tests during build

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        # If the logger module was enabled, add logger library
        if self.options.enable_logger:
            self.cpp_info.libs.append("stdx_logger")
        # The flag module is header-only; no library needed
