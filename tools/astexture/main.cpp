#include <array>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../../src/io/io.h"
#include "../../src/io/io_archive.h"

#include "../../src/job/job.h"

#include "../../src/util/util_error.h"
#include "../../src/util/util_log.h"

#include "texture.h"

using namespace as;

/**
 * \brief Command line argument buffer
 */
class ArgBuffer {

public:

  ArgBuffer() { }
  ArgBuffer(int argc, char** argv)
  : m_argv(argv), m_argc(argc) { }

  std::string next() {
    if (m_pos == m_argc)
      return std::string();

    return std::string(m_argv[m_pos++]);
  }

  std::string getAppName() const {
    return std::string(m_argv[0]);
  }

  operator bool() const {
    return m_pos < m_argc;
  }

private:

  char* const*  m_argv = nullptr;
  uint32_t      m_argc = 0;
  uint32_t      m_pos  = 1;

};


/**
 * \brief Texture conversion app
 *
 * Deals with all the user-facing stuff
 */
class TextureApp {

public:

  explicit TextureApp(
          ArgBuffer                     args)
  : m_args    (std::move(args))
  , m_jobs    (std::thread::hardware_concurrency())
  , m_io      (IoBackend::eDefault, 1) {
    
  }


  int run() {
    if (!processArgs()
     || !processInputs()
     || !writeOutput())
      return 1;

    return 0;
  }


private:

  ArgBuffer m_args;

  Jobs      m_jobs;
  Io        m_io;

  std::vector<std::shared_ptr<Texture>> m_textures;

  std::optional<std::filesystem::path> m_outputPath;

  void addTexture(
          TextureArgs&                  args) {
    if (args.files.empty())
      return;

    m_textures.push_back(std::make_shared<Texture>(m_io, m_jobs, args));

    // Preserve all other options
    args.name.clear();
    args.files.clear();
  }


  bool processInputs() {
    for (auto& t : m_textures) {
      if (!t->process())
        return false;
    }

    return true;
  }


  bool writeOutput() {
    IoArchiveDesc desc;

    for (auto& t : m_textures)
      desc.files.push_back(t->getFileDesc());

    IoArchiveBuilder builder(m_io, m_jobs, desc);
    return builder.build(*m_outputPath) == IoStatus::eSuccess;
  }


  bool processArgs() {
    TextureArgs args;
    bool arrayMode = false;

    while (m_args) {
      std::string arg = m_args.next();

      if (arg == "-h" || arg == "--help") {
        printHelp();
        return 1;
      } else if (arg == "-o" || arg == "--output") {
        if (!m_args) {
          Log::err("Missing output file path");
          return false;
        }

        if (m_outputPath) {
          Log::err("Output file already specified");
          return false;
        }

        m_outputPath = m_args.next();
      } else if (arg == "-n" || arg == "--name") {
        args.name = arg;
      } else if (arg == "-m" || arg == "--mips") {
        auto option = parseToggle(m_args.next());

        if (option)
          args.enableMips = *option;
      } else if (arg == "-a" || arg == "--array") {
        addTexture(args);

        arrayMode = true;
        args.enableCube = false;
      } else if (arg == "-c" || arg == "--cube") {
        addTexture(args);

        arrayMode = true;
        args.enableCube = true;
      } else if (arg == "-s" || arg == "--single") {
        addTexture(args);

        arrayMode = false;
        args.enableCube = false;
      } else if (arg == "-f" || arg == "--format") {
        auto format = parseFormat(m_args.next());

        if (format)
          args.format = *format;
      } else if (arg == "--allow-compression") {
        auto option = parseToggle(m_args.next());

        if (option)
          args.allowCompression = *option;
      } else if (arg == "--allow-bc7") {
        auto option = parseToggle(m_args.next());

        if (option)
          args.allowBc7 = *option;
      } else {
        args.files.push_back(arg);

        if (!arrayMode)
          addTexture(args);
      }
    }

    addTexture(args);

    if (!m_outputPath) {
      Log::err("No output file specified.");
      return false;
    }

    return true;
  }


  void printHelp() {
    std::cout << "Usage: " << m_args.getAppName() << " -o outfile.asa [options infile1 [infile2 ...]]" << std::endl << std::endl
              << "General options:" << std::endl
              << "  -h                  Print this help" << std::endl
              << "  -o  --output file   Sets output file" << std::endl
              << "  -n  --name name     Overrides the name of the next texture. By default, " << std::endl
              << "                      the file name will be used, excluding the extension." << std::endl << std::endl
              << "  -m  --mips on|off   Enables or disables mip-mapping for subsequent textures." << std::endl
              << "                      Defaults to on." << std::endl << std::endl
              << "  -a  --array         Enables array mode. In array mode, subsequent inputs will be" << std::endl
              << "                      packed into one array texture, and must all have the same" << std::endl
              << "                      format and dimensions." << std::endl << std::endl
              << "  -c  --cube          Enables cube map mode. Works the same way as array mode, but" << std::endl
              << "                      also sets a flag to make the texture cube map compatible." << std::endl
              << "                      Requires that all inputs have square dimensions, and that the" << std::endl
              << "                      number of input textures is a multiple of 6." << std::endl << std::endl
              << "  -s  --single        Disables array or cube map mode and packs each input into" << std::endl
              << "                      separate textures." << std::endl
              << "  -f  --format auto|bc1|bc3|bc4|bc5|bc7|r8|rg8|rgba8" << std::endl
              << "                      Sets the format to use for subsequent textures. If set to auto," << std::endl
              << "                      a format is chosen based on image properties and the presence" << std::endl
              << "                      of --allow-compression or --allow-bc7 options. Defaults to auto." << std::endl << std::endl
              << "  --allow-compression on|off" << std::endl
              << "                      Defines whether to use block-compressed or raw formats for" << std::endl
              << "                      subsequent textures using the auto format. Defaults to on." << std::endl
              << "                      Has no effect on textures that have a format specified." << std::endl << std::endl
              << "  --allow-bc7 on|off" << std::endl
              << "                      Defines whether to use the BC7 format in favour of BC1 or" << std::endl
              << "                      BC3 for subsequent textures using the auto format. Defaults" << std::endl
              << "                      to off. Has no effect on textures that have a format specified." << std::endl << std::endl
              << "Examples:" << std::endl << std::endl
              << "  " << m_args.getAppName() << " -o out.asa -f bc7 color.png -f bc5 normal.png" << std::endl << std::endl
              << "  Creates an archive using one BC7-compressed color texture and one BC5-compressed" << std::endl
              << "  normal map, including automatic mip map generation." << std::endl << std::endl
              << "  " << m_args.getAppName() << " -o out.asa -n first -a first/*.png -n second -a second/*.png" << std::endl << std::endl
              << "  Creates an archive containing two texture arrays. Note that -a is set a second" << std::endl
              << "  time in order to denote the end of the first array." << std::endl << std::endl;
  }


  static std::optional<bool> parseToggle(
    const std::string&                  arg) {
    if (arg == "on")  return true;
    if (arg == "off") return false;

    Log::warn("'", arg, "' not a valid option");
    return std::nullopt;
  }


  static std::optional<GfxFormat> parseFormat(
    const std::string&                  arg) {
    static const std::array<std::pair<const char*, GfxFormat>, 9> s_map = {{
      { "auto",   GfxFormat::eUnknown       },
      { "bc1",    GfxFormat::eBc1srgb       },
      { "bc3",    GfxFormat::eBc3srgb       },
      { "bc4",    GfxFormat::eBc4un         },
      { "bc5",    GfxFormat::eBc5un         },
      { "bc7",    GfxFormat::eBc7srgb       },
      { "r8",     GfxFormat::eR8un          },
      { "rg8",    GfxFormat::eR8G8un        },
      { "rgba8",  GfxFormat::eR8G8B8A8srgb  },
    }};

    for (const auto& e : s_map) {
      if (arg == e.first)
        return e.second;
    }

    Log::warn("'", arg, "' not a valid format");
    return std::nullopt;
  }

};


int main(int argc, char** argv) {
  Log::setLogLevel(LogSeverity::eWarn);

  try {
    TextureApp app(ArgBuffer(argc, argv));
    return app.run();
  } catch (const Error& e) {
    Log::err(e.what());
    return 1;
  }
}
