class NothingBrowser < Formula
  desc "Does nothing... except everything that matters. A meta scrapper browser."
  homepage "https://github.com/BunElysiaReact/nothing-browser"
  url "https://github.com/BunElysiaReact/nothing-browser/archive/refs/tags/v0.1.0.tar.gz"
  sha256 "REPLACE_WITH_REAL_SHA256_AFTER_TAGGING"
  license "MIT"
  version "0.1.0"

  depends_on "cmake" => :build
  depends_on "qt@6"

  def install
    mkdir "build" do
      system "cmake", "..",
             "-DCMAKE_BUILD_TYPE=Release",
             "-DCMAKE_PREFIX_PATH=#{Formula["qt@6"].opt_prefix}",
             *std_cmake_args
      system "make", "-j#{ENV.make_jobs}"
      bin.install "nothing-browser"
    end
    # Icon
    (share/"nothing-browser").install "assets/icons/logo.svg"
  end

  def caveats
    <<~EOS
      Nothing Browser is a scrapper-first browser.
      Do NOT use it for banking or Google services — you will be blocked.

      Run: nothing-browser
    EOS
  end

  test do
    system "#{bin}/nothing-browser", "--version"
  end
end