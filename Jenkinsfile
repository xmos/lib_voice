// This file relates to internal XMOS infrastructure and should be ignored by external users

@Library('xmos_jenkins_shared_library@v0.54.0') _

def runningOn(machine) {
  println "Stage running on:"
  println machine
}

// Runs a single pytest suite, wrapping the repeated dir/env/junit boilerplate.
// archOpt is "" or "--arch <name>" for suites that support it. postSteps (if given) runs
// after junit reporting, still inside the suite's dir()/env context (e.g. for archiveArtifacts
// or a follow-up python script).
def runSuite(String suiteDir, String archOpt, String pytestArgs, Closure postSteps = null) {
  dir(suiteDir) {
    def cmd = "pytest ${archOpt} ${pytestArgs} -v --durations=0 --junitxml=pytest_result.xml".replaceAll(/\s+/, ' ').trim()
    withEnv(["hydra_audio_PATH=/projects/hydra_audio"]) {
      try {
        sh cmd
      } finally {
        junit "pytest_result.xml"
      }
      if (postSteps) { postSteps() }
    }
  }
}

// Runs every test suite built for the given architecture ("xs3a", "vx4b" or "native").
// Must be called from within dir("tests") { withTools(...) { withVenv { ... } } }.
// Each suite is individually catchError-wrapped so one failure doesn't skip the rest.
def test_arch(String archName) {
  if (archName == 'native') {
    sh "cmake -B build_xcommon_cmake" // to fetch lib_xcore_math
    // Only suites with an existing native build/execution path are run natively.
    catchError(stageResult: 'UNSTABLE', catchInterruptions: false) {
      runSuite("lib_vnr/vnr_unit_tests", "--arch native", "-n 2")
    }
    catchError(stageResult: 'UNSTABLE', catchInterruptions: false) {
      runSuite("lib_ic/test_calc_vnr_pred", "--arch native", "-n 2")
    }
    catchError(stageResult: 'FAILURE', catchInterruptions: false) {
      dir("lib_vnr/test_vnr_cffi") {
        sh "python build_vnr_cffi.py"
      }
      runSuite("lib_vnr/test_vnr_cffi", "--arch native", "-n 4")
    }
    catchError(stageResult: 'FAILURE', catchInterruptions: false) {
      dir("lib_ic/test_ic_cffi") {
        sh "python build_ic_cffi.py"
      }
      runSuite("lib_ic/test_ic_cffi", "--arch native", "-s")
    }
    catchError(stageResult: 'FAILURE', catchInterruptions: false) {
      dir("stage_b") {
        sh "python build_stage_b_cffi.py"
      }
      // -n 2: both tests now write to pytest's per-test tmp_path, safe to run concurrently
      runSuite("stage_b", "--arch native", "-n 2")
    }
    return
  }

  def arch = "--arch ${archName}"

  if (archName == 'vx4b') {
    // fails loading xinterpreters on ubuntu 22
    echo "Skipping lib_vnr/vnr_unit_tests on vx4b: fails loading xinterpreters on ubuntu 22"
  } else {
    catchError(stageResult: 'FAILURE', catchInterruptions: false) {
      runSuite("lib_vnr/vnr_unit_tests", arch, "-n 2")
    }
  }
  catchError(stageResult: 'FAILURE', catchInterruptions: false) {
    runSuite("lib_ns", arch, "-n 2")
  }
  catchError(stageResult: 'FAILURE', catchInterruptions: false) {
    runSuite("lib_ic", arch, "-n 2") {
      sh "python test_ic_spec/print_stats.py > ic_spec_summary_${archName}.txt"
    }
  }

  catchError(stageResult: 'FAILURE', catchInterruptions: false) {
    runSuite("lib_adec", arch, "-n 2") {
      sh "python test_delay_estimator/print_stats.py"
    }
  }

  catchError(stageResult: 'FAILURE', catchInterruptions: false) {
    runSuite("lib_aec", arch, "-n 2")
  }

  catchError(stageResult: 'FAILURE', catchInterruptions: false) {
    runSuite("lib_agc", arch, "-n 2")
  }

  catchError(stageResult: 'FAILURE', catchInterruptions: false) {
    runSuite("profile_mips", arch, "-n 2") {
      archiveArtifacts artifacts: "lib_voice_mips.json", fingerprint: true, onlyIfSuccessful: true
    }
  }
  if (archName == 'vx4b') {
    // fails loading xinterpreters on ubuntu 22
    echo "Skipping profile memory test as no memory prints"
  } else {
    catchError(stageResult: 'FAILURE', catchInterruptions: false) {
      runSuite("profile_memory", arch, "-n 2") {
        archiveArtifacts artifacts: "lib_voice_memory.json", fingerprint: true, onlyIfSuccessful: true
      }
    }
  }
}

// Runs one architecture's whole Verification stage (Get View, XTAG reset, tests, pipeline,
// artifact archiving, cleanup) - shared by vx4b/native/xs3a instead of 3 near-identical
// declarative stage blocks. `cfg` fields: agentLabel, toolsVersion, archName,
// unstashNames, hwTarget (null skips XTAG reset), archiveAlways/
// archiveFailure (closures, optional).
def runVerification(Map cfg) {
  if (env.GH_LABEL_DOC_ONLY.toBoolean()) {
    return
  }
  node(cfg.agentLabel) {
    try {
      stage('Get View') {
        runningOn(env.NODE_NAME)
        sh "git clone --depth 1 --branch main git@github.com:xmos/amazon_wwe.git"
        sh "git clone --depth 1 --branch master git@github.com:xmos/sensory_sdk.git"
        dir("${env.REPO}") {
          checkout scm
          dir("tests") {
            withTools(cfg.toolsVersion) {
              createVenv(reqFile: "requirements_test.txt")
            }
            cfg.unstashNames.each { unstash it }
          }
        }
      } // Get View

      if (cfg.hwTarget) {
        stage('Reset XTAGs') {
          dir("${env.REPO}/tests") {
            sh 'rm -f ~/.xtag/acquired' // Hacky but ensure it always works even when previous failed run left lock file present
            withTools(cfg.toolsVersion) {
              withVenv {
                sh "xtagctl reset_all ${cfg.hwTarget}"
              }
            }
          }
        }
      }

      stage('tests') {
        dir("${env.REPO}/tests") {
          withTools(cfg.toolsVersion) {
            withVenv {
              test_arch(cfg.archName)
            }
          }
        }
      } // tests
      if (cfg.archName != 'native') {
        stage('Pipeline tests') {
          catchError(stageResult: 'FAILURE', catchInterruptions: false) {
            dir("${env.REPO}/tests/pipeline") {
              withEnv(["hydra_audio_PATH=/projects/hydra_audio"]) {
                withEnv(["PIPELINE_FULL_RUN=${env.PIPELINE_FULL_RUN}", "SENSORY_PATH=${env.WORKSPACE}/sensory_sdk/", "AMAZON_WWE_PATH=${env.WORKSPACE}/amazon_wwe/"]) {
                  withTools(cfg.toolsVersion) {
                    withVenv {
                      echo "PIPELINE_FULL_RUN set as " + env.PIPELINE_FULL_RUN
                      // NOTE: "--arch ${cfg.archName} python" (space-separated) is required, not
                      // "--arch=${cfg.archName} python" - with "=", argparse's nargs="+" only
                      // consumes "${cfg.archName}" and "python" becomes a bogus positional pytest
                      // path argument ("ERROR: file or directory not found: python").
                      sh "pytest -n 4 --arch ${cfg.archName} python --junitxml=pytest_result.xml -vv"
                      junit "pytest_result.xml"
                      def threshold = (cfg.archName == 'vx4b') ? 3 : 1
                      sh "python compare_keywords.py results_Avona_aec_ic_ns_agc_prev_arch_${cfg.archName}.csv results_Avona_aec_ic_ns_agc_prev_arch_python.csv --pass-threshold=${threshold}"
                    }
                  }
                }
              }
            }
          }
        } // Pipeline tests

        stage('Benchmark Pipeline tests results') {
          if (env.PIPELINE_FULL_RUN == "1") {
            dir("${env.REPO}/tests/pipeline") {
              withTools(cfg.toolsVersion) {
                withVenv {
                  copyArtifacts filter: '**/results_*.csv', fingerprintArtifacts: true, projectName: '../lib_audio_pipelines/master', selector: lastSuccessful()
                  runPython("python plot_results.py lib_audio_pipelines/tests/pipelines/results_lib_ap_prev_arch_xcore.csv results_Avona_prev_arch_${cfg.archName}.csv --single-plot --ww-column='0_2 1_2' --figname=results_benchmark_prev_arch")
                  runPython("python plot_results.py lib_audio_pipelines/tests/pipelines/results_lib_ap_alt_arch_xcore.csv results_Avona_alt_arch_${cfg.archName}.csv --single-plot --ww-column='0_2 1_2' --figname=results_benchmark_alt_arch")
                }
              }
            }
          }
        }
      }
    } finally {
      // Mirrors declarative post{always{}}/post{failure{}}: always archive, and additionally
      // archive failure-only debug artifacts if this build's result looks unhealthy. Since
      // parallel branches share one build result, a failure in a sibling branch can also trigger
      // this - an accepted, harmless over-approximation for a debug-artifact convenience feature.
      try {
        if (cfg.archiveAlways) {
          cfg.archiveAlways()
        }
        if (cfg.archiveFailure && currentBuild.currentResult in ['FAILURE', 'UNSTABLE']) {
          cfg.archiveFailure()
        }
      } finally {
        xcoreCleanSandbox()
      }
    }
  }
}

// Shared archiveAlways/archiveFailure closures for runVerification's vx4b/xs3a branches.
// Filenames/dirs are arch-suffixed (ic_spec_summary_<arch>.txt, keyword_input_<p_arch>_<arch>/...,
// results_..._<arch>.csv) so both branches can archive from these same patterns without collisions.
def archiveResultsAlways() {
  archiveArtifacts artifacts: "${env.REPO}/tests/lib_ic/ic_spec_summary_*.txt", fingerprint: true
  archiveArtifacts artifacts: "${env.REPO}/tests/pipeline/**/results_*.csv", fingerprint: true
  archiveArtifacts artifacts: "${env.REPO}/tests/pipeline/**/results_*.png", fingerprint: true, allowEmptyArchive: true
  archiveArtifacts artifacts: "${env.REPO}/tests/pipeline/keyword_input_*/*.npy", fingerprint: true, allowEmptyArchive: true
}

def archiveResultsFailure() {
  // archive wavs on failure only
  archiveArtifacts artifacts: "${env.REPO}/tests/pipeline/keyword_input_*/*.wav", fingerprint: true, allowEmptyArchive: true
}

getApproval()

pipeline {
  agent none

  parameters {
    string(
      name: 'TOOLS_VERSION',
      defaultValue: '15.3.1',
      description: 'The XTC tools version'
    )
    string(
      name: 'TOOLS_VX4_VERSION',
      defaultValue: '-j --repo arch_vx_slipgate -b develop -a XTC 1184',
      description: 'The XTC Slipgate tools version'
    )
    string(
      name: 'XMOSDOC_VERSION',
      defaultValue: 'v8.1.0',
      description: 'The xmosdoc version'
    )
    string(
      name: 'INFR_APPS_VERSION',
      defaultValue: 'v3.5.0',
      description: 'The infr_apps version'
    )
    booleanParam(name: 'FULL_TEST_OVERRIDE',
                 defaultValue: false,
                 description: 'Force a full test. This increases the number of iterations/scope in some tests')
    booleanParam(name: 'PIPELINE_FULL_RUN',
                 defaultValue: false,
                 description: 'Enables pipelines characterisation test which takes 5.0hrs by itself. Normally run nightly')
  }
  environment {
    REPO = 'lib_voice'
    FULL_TEST = """${(params.FULL_TEST_OVERRIDE
                    || env.BRANCH_NAME == 'develop'
                    || env.BRANCH_NAME == 'main'
                    || env.BRANCH_NAME ==~ 'release/.*') ? 1 : 0}"""
    PIPELINE_FULL_RUN = """${params.PIPELINE_FULL_RUN ? 1 : 0}"""
  }
  options {
    skipDefaultCheckout()
    timestamps()
    buildDiscarder(xmosDiscardBuildSettings(onlyArtifacts=false))
  }
  stages {
    stage('Build and Docs') {
      parallel {
        stage('Examples, docs, repo checks') {
          agent {
            label "documentation&&x86_64&&linux"
          }
          stages {
            stage("Examples build") {
              steps {
                runningOn(env.NODE_NAME)

                dir("${REPO}") {
                  checkoutScmShallow()
                  createVenv(reqFile: "requirements.txt")
                }
                dir("${REPO}/examples") {
                  withVenv {
                    // will use TOOLS_VERSION which is valid for an xs3a build
                    xcoreBuild(archiveBins: false, buildDir: "build_xs3a")
                    // setting TOOLS_VERSION to be the vx4b tools
                    withEnv(["TOOLS_VERSION=${params.TOOLS_VX4_VERSION}"]) {
                      xcoreBuild(archiveBins: false, buildDir: "build_vx4b", cmakeOpts: "-DAPP_HW_TARGET=XK-EVK-XU416")
                    }
                  }
                }
              }
            } // Examples build

            stage("Repo checks") {
              steps {
                warnError("Repo checks failed") {
                  runRepoChecks(
                    repo_dir:"${WORKSPACE}/${REPO}",
                    reqFile:"${WORKSPACE}/${REPO}/requirements.txt"
                  )
                }
              }
            } // Repo checks

            stage("Docs build") {
              steps {
                dir("${REPO}") {
                  warnError("Docs build failed") {
                    buildDocs()
                  }
                }
              }
            } // Docs build

            stage("Archive Lib") {
              steps {
                archiveSandbox(REPO)
              }
            } //stage("Archive Lib")

          } // stages

          post {
            cleanup {
              xcoreCleanSandbox()
            }
          }
        } // Build and Docs

        stage('vx4b build') {
          when {
            expression { !env.GH_LABEL_DOC_ONLY.toBoolean() }
          }
          agent {
            label 'x86&&linux'
          }
          stages {
            stage('Get View') {
              steps {
                runningOn(env.NODE_NAME)

                dir("${REPO}") {
                  checkout scm
                  // need ai_tools for the build
                  // need numpy to generate aec tests, will get in from ai_tools
                  createVenv(reqFile: "requirements.txt")
                }
              }
            } // Get View

            stage('Build tests vx4b') {
              steps {
                dir("${REPO}") {
                  withVenv {
                    withEnv(["TOOLS_VERSION=${params.TOOLS_VX4_VERSION}"]) {
                      dir("tests") {
                        script {
                          def speedupOpt = (env.FULL_TEST == "1") ? "" : "-DTEST_SPEEDUP_FACTOR=4"
                          sh "xrun -l"
                          xcoreBuild(buildDir: "build_vx4b", archiveBins: false, cmakeOpts: "${speedupOpt} -DAPP_HW_TARGET=XK-EVK-XU416")
                        }
                        stash name: 'vx4b_build_xcore', includes: '**/bin/**/*.xe'
                      }
                    }
                  }
                }
              }
            } // Build tests vx4b

          } // stages

          post {
            cleanup {
              xcoreCleanSandbox()
            }
          }
        }

        stage('xs3a build, PartA') {
          when {
            expression { !env.GH_LABEL_DOC_ONLY.toBoolean() }
          }
          agent {
            label 'x86_64&&linux'
          }
          stages {
            stage('Get view') {
              steps {
                runningOn(env.NODE_NAME)

                dir("${REPO}") {
                  checkout scm
                  // need ai_tools for the build
                  // need numpy to generate aec tests, will get in from ai_tools
                  createVenv(reqFile: "requirements.txt")
                }
              }
            }
            stage('Build tests, xcommon-cmake xcore build, partA') {
              steps {
                dir("${REPO}") {
                    withTools(params.TOOLS_VERSION) {
                      withVenv {
                        dir("tests") {
                          script {
                            def speedupOpt = (env.FULL_TEST == "1") ? "" : "-DTEST_SPEEDUP_FACTOR=4"
                            xcoreBuild(buildDir: "build_xcommon_cmake", archiveBins: false, cmakeOpts: "${speedupOpt} -DTEST_BUILD_PART=partA")
                          }
                          stash name: 'xcommon_cmake_build_xcore_partA', includes: '**/bin/**/*.xe'
                        }
                      }
                    }
                }
              }
            }
            stage('Build tests, xcommon-cmake native build') {
              steps {
                dir("${REPO}") {
                    withTools(params.TOOLS_VERSION) {
                      withVenv {
                        dir("tests") {
                          xcoreBuild(buildDir: "build_xcommon_cmake_native", archiveBins: false, cmakeOpts: "-DBUILD_NATIVE=ON")
                          stash name: 'xcommon_cmake_build_native', includes: '**/bin/**/', excludes: '**/bin/**/*.xe'
                        }
                      }
                    }
                }
              }
            }
            stage('Custom CMake build') {
              steps {
                sh "git clone git@github.com:xmos/xmos_cmake_toolchain.git --depth 1 --branch v1.0.0"
                // Do custom cmake, xcore build, from the tests/custom_cmake_build directory
                dir("${REPO}/tests/custom_cmake_build") {
                  withTools(params.TOOLS_VERSION) {
                    withVenv {
                      sh 'cmake -B build --toolchain=../../../xmos_cmake_toolchain/xs3a.cmake'
                      sh 'make -C build -j$(nproc)'
                   }
                  }
                }
              }
            }
          }
          post {
            cleanup {
              xcoreCleanSandbox()
            }
          }
        } // xs3a build, PartA

        stage('xs3a build, PartB') {
          when {
            expression { !env.GH_LABEL_DOC_ONLY.toBoolean() }
          }
          agent {
            label 'x86_64&&linux'
          }
          stages {
            stage('Get view') {
              steps {
                runningOn(env.NODE_NAME)

                dir("${REPO}") {
                  checkout scm
                  // need ai_tools for the build
                  // need numpy to generate aec tests, will get in from ai_tools
                  createVenv(reqFile: "requirements.txt")
                }
              }
            }
            stage('Build tests, xcommon-cmake xcore build, PartB') {
              steps {
                dir("${REPO}") {
                    withTools(params.TOOLS_VERSION) {
                      withVenv {
                        dir("tests") {
                          script {
                            def speedupOpt = (env.FULL_TEST == "1") ? "" : "-DTEST_SPEEDUP_FACTOR=4"
                              xcoreBuild(buildDir: "build_xcommon_cmake", archiveBins: false, cmakeOpts: "${speedupOpt} -DTEST_BUILD_PART=partB")
                          }
                          stash name: 'xcommon_cmake_build_xcore_partB', includes: '**/bin/**/*.xe'
                        }
                      }
                    }
                }
              }
            }
          }
          post {
            cleanup {
              xcoreCleanSandbox()
            }
          }
        } // xs3a build, PartB
      } // parallel
    } // Build and Docs

    stage("Testing") {
      agent none
      steps {
        script {
          parallel(
            'vx4b Verification': {
              runVerification([
                agentLabel: 'vx4',
                toolsVersion: params.TOOLS_VX4_VERSION,
                archName: 'vx4b',
                unstashNames: ['vx4b_build_xcore'],
                hwTarget: 'XK-EVK-XU416',
                archiveAlways: { archiveResultsAlways() },
                archiveFailure: { archiveResultsFailure() },
              ])
            },
            'native and CFFI Verification': {
              runVerification([
                agentLabel: 'x86_64&&linux&&!noAVX2',
                toolsVersion: params.TOOLS_VERSION,
                archName: 'native',
                unstashNames: ['xcommon_cmake_build_native'],
                hwTarget: null,
              ])
            },
            'xs3a Verification': {
              runVerification([
                agentLabel: 'xcore.ai',
                toolsVersion: params.TOOLS_VERSION,
                archName: 'xs3a',
                unstashNames: ['xcommon_cmake_build_xcore_partA', 'xcommon_cmake_build_xcore_partB'],
                hwTarget: 'XCORE-AI-EXPLORER',
                archiveAlways: { archiveResultsAlways() },
                archiveFailure: { archiveResultsFailure() },
              ])
            },
          )
        }
      }
    } // Testing

    stage('🚀 Release') {
      when {
      expression { triggerRelease.isReleasable() }
      }
      steps {
        triggerRelease()
      }
    } // stage('🚀 Release')

  } // stages
} // pipeline
