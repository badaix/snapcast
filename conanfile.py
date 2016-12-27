from conans import ConanFile, CMake

class SnapcastConan(ConanFile):
		settings = "os", "compiler", "build_type", "arch"
		requires = "ogg/1.3.2@coding3d/stable", "vorbis/1.3.5@coding3d/stable", "FLAC/1.3.1@Outurnate/stable", "Asio/1.10.6@fmorgner/stable", "asound/1.1.2@Outurnate/stable", "avahi/0.6.32@Outurnate/stable"
		generators = "cmake", "txt", "env"

		def imports(self):
				self.copy("*.dll", dst="bin", src="bin") # From bin to bin
				self.copy("*.dylib*", dst="bin", src="lib") # From lib to bin
		
		def build(self):
				cmake = CMake(self.settings)
				self.run('cmake "%s" %s' % (self.conanfile_directory, cmake.command_line))
				self.run('cmake --build . %s' % cmake.build_config)
