import java.util.Properties

plugins {
    id("com.android.application")
}

// Release signing: android/keystore.properties (not in git) names your keystore:
//   storeFile=/path/to/galaxians.jks
//   storePassword=...
//   keyAlias=galaxians
//   keyPassword=...
// Without it the release APK is signed with the debug key, which installs fine
// but cannot be updated in place by a build signed with a different key.
val keystoreFile = rootProject.file("keystore.properties")
val keystore = Properties().apply {
    if (keystoreFile.exists())
        keystoreFile.inputStream().use { load(it) }
}

android {
    namespace = "com.galaxiansremake.galaxians"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "com.galaxiansremake.galaxians"
        // Android 13: the first release whose Vulkan loader has the 1.3 entry points.
        minSdk = 33
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        ndk {
            // x86_64 is for the emulator.
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_shared")
            }
        }
    }

    signingConfigs {
        if (keystoreFile.exists()) {
            create("release") {
                storeFile = file(keystore.getProperty("storeFile"))
                storePassword = keystore.getProperty("storePassword")
                keyAlias = keystore.getProperty("keyAlias")
                keyPassword = keystore.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.findByName("release") ?: signingConfigs.getByName("debug")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
            version = "3.31.6"
        }
    }

    sourceSets {
        getByName("main") {
            // SDL's Java half, from the same submodule CMake builds the native half from.
            java.directories.add("../../external/SDL/android-project/app/src/main/java")
            assets.directories.add("../../assets")
        }
    }
}
