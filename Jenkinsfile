// This file relates to internal XMOS infrastructure and should be ignored by external users

@Library('xmos_jenkins_shared_library@v0.46.0') _

def runningOn(machine) {
  println "Stage running on:"
  println machine
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
      defaultValue: '-j --repo arch_vx_slipgate -b master -a XTC 112',
      description: 'The XTC Slipgate tools version'
    )
    string(
      name: 'XMOSDOC_VERSION',
      defaultValue: 'v8.0.1',
      description: 'The xmosdoc version'
    )
    string(
      name: 'INFR_APPS_VERSION',
      defaultValue: 'v3.3.0',
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
                    xcoreBuild(archiveBins: false, buildDir: "build_xs3a", toolsVersion: params.TOOLS_VERSION)
                    xcoreBuild(archiveBins: false, buildDir: "build_vx4b", toolsVersion: params.TOOLS_VX4_VERSION, cmakeOpts: "-DXCORE_TARGET=XK-EVK-XU416")
                  }
                }
              }
            } // Examples build

            stage("Repo checks") {
              steps {
                // Hack to get the changelog checker to install ai_tools before doing cmake
                script {
                  dir("${WORKSPACE}/.infr") {
                    // Check out the infr_apps repo and dependencies
                    if (!fileExists("infr_apps")) {
                      sh "git clone --branch '${params.INFR_APPS_VERSION}' git@github.com:xmos/infr_apps"
                    }
                    dir("infr_apps") {
                      if (!fileExists(".venv")) {
                        createVenv(reqFile: "requirements.txt")
                      }
                      withVenv {
                        sh "pip install -r ${WORKSPACE}/${REPO}/requirements.txt"
                      }
                    }
                  }
                }
                warnError("Repo checks failed") {
                  runRepoChecks("${WORKSPACE}/${REPO}")
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
        }
        stage('xcore.ai executables build, PartA') {
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
                            if (env.FULL_TEST == "1") {
                              xcoreBuild(buildDir: "build_xcommon_cmake", archiveBins: false, cmakeOpts: "-DTEST_BUILD_PART=partA")
                            }
                            else {
                              xcoreBuild(buildDir: "build_xcommon_cmake", archiveBins: false, cmakeOpts: "-DTEST_SPEEDUP_FACTOR=4 -DTEST_BUILD_PART=partA")
                            }
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
        }
        stage('xcore.ai executables build, PartB') {
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
                            if (env.FULL_TEST == "1") {
                              xcoreBuild(buildDir: "build_xcommon_cmake", archiveBins: false, cmakeOpts: "-DTEST_BUILD_PART=partB")
                            }
                            else {
                              xcoreBuild(buildDir: "build_xcommon_cmake", archiveBins: false, cmakeOpts: "-DTEST_SPEEDUP_FACTOR=4 -DTEST_BUILD_PART=partB")
                            }
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
        }
      }
    }
    stage('xcore.ai Verification') {
      when {
        expression { !env.GH_LABEL_DOC_ONLY.toBoolean() }
      }
      agent {
        label 'xcore.ai'
      }
      stages{
        stage('Get View') {
          steps {
            runningOn(env.NODE_NAME)

            sh "git clone --depth 1 --branch main git@github.com:xmos/amazon_wwe.git"
            sh "git clone --depth 1 --branch master git@github.com:xmos/sensory_sdk.git"

            dir("${REPO}") {
              checkout scm
              dir("tests") {
                createVenv(reqFile: "requirements_test.txt")
              }
            }
          }
        }
        stage('Make/get bins and libs'){
          steps {
            dir("${REPO}/tests") {
              withTools(params.TOOLS_VERSION) {
                withVenv {

                  sh "cmake -B build_xcommon_cmake" // to fetch lib_xcore_math

                  // Build x86 versions locally as we had problems with moving bins and libs over from previous build due to brew
                  dir("custom_cmake_build") {
                    sh "cmake --version"
                    sh 'cmake -B build'
                    sh 'make -C build -j$(nproc)'
                  }
                  // We do this again on the NUCs for verification later, but this just checks we have no build error
                  dir("lib_ic/py_c_frame_compare") {
                    sh "python build_ic_frame_proc.py"
                  }
                  // We do this again on the NUCs for verification later, but this just checks we have no build error
                  dir("lib_vnr/test_vnr_cffi") {
                    sh "python build_vnr_cffi.py"
                  }
                  dir("stage_b") {
                    sh "python build_c_code.py"
                  }
                  unstash 'xcommon_cmake_build_xcore_partA'
                  unstash 'xcommon_cmake_build_xcore_partB'
                  unstash 'xcommon_cmake_build_native'
                }
              }
            }
          }
        }
        stage('Reset XTAGs'){
          steps{
            dir("${REPO}/tests") {
              sh 'rm -f ~/.xtag/acquired' // Hacky but ensure it always works even when previous failed run left lock file present
              withTools(params.TOOLS_VERSION) {
                withVenv{
                  sh "xtagctl reset_all XCORE-AI-EXPLORER"
                }
              }
            }
          }
        }

        stage('MIPS are memory resource usage tests') {
          steps {
            catchError(stageResult: 'FAILURE', catchInterruptions: false) {
              dir("${REPO}/tests") {
                withTools(params.TOOLS_VERSION) {
                  withVenv {
                    dir("profile_memory") {
                      sh "pytest -n 1 --junitxml=pytest_result.xml"
                      junit "pytest_result.xml"
                      archiveArtifacts artifacts: "lib_voice_memory.json", fingerprint: true, onlyIfSuccessful: true
                    }
                    withEnv(["hydra_audio_PATH=/projects/hydra_audio"]) {
                      dir("profile_mips") {
                        sh "pytest -n 2 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                        archiveArtifacts artifacts: "lib_voice_mips.json", fingerprint: true, onlyIfSuccessful: true
                      }
                    }
                  }
                }
              }
            }
          }
        }

        stage('VNR tests') {
          steps {
            catchError(stageResult: 'FAILURE', catchInterruptions: false){
              dir("${REPO}/tests/lib_vnr") {
                withTools(params.TOOLS_VERSION) {
                  withVenv {
                    withEnv(["hydra_audio_PATH=/projects/hydra_audio"]) {
                      dir("vnr_unit_tests") {
                        sh "pytest -n 2 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                      dir("test_vnr_cffi") {
                        sh "python build_vnr_cffi.py"
                        sh "pytest -n 4 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                    }
                  }
                }
              }
            }
          }
        }

        stage('NS tests') {
          steps {
            catchError(stageResult: 'FAILURE', catchInterruptions: false){
              dir("${REPO}/tests/lib_ns") {
                withTools(params.TOOLS_VERSION) {
                  withVenv {
                    withEnv(["hydra_audio_PATH=/projects/hydra_audio"]) {
                      dir("compare_c_py"){
                        sh "pytest -n 2 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                      dir("ns_unit_tests"){
                        sh "pytest -n 1 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                    }
                  }
                }
              }
            }
          }
        }

        stage('IC tests') {
          steps {
            catchError(stageResult: 'FAILURE', catchInterruptions: false){
              dir("${REPO}/tests/lib_ic") {
                withTools(params.TOOLS_VERSION) {
                  withVenv {
                    withEnv(["hydra_audio_PATH=/projects/hydra_audio"]) {
                      dir("ic_unit_tests"){
                        sh "pytest -n 2 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                      dir("py_c_frame_compare"){
                        sh "python build_ic_frame_proc.py"
                        sh "pytest -s --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                      dir("test_ic_spec"){
                        // This test compares the model and C implementation over a range of scenarious for:
                        // convergence_time, db_suppression, maximum noise added to input (to test for stability)
                        // and expected group delay. It will fail if these are not met.
                        sh "pytest -n 2 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                        sh "python print_stats.py > ic_spec_summary.txt"
                        // This script generates a number of polar plots of attenuation vs null point angle vs freq
                        // It currently only uses the python model to do this. It takes about 40 mins for all plots
                        // and generates a series of IC_performance_xxxHz.svg files which could be archived
                        //sh "python plot_ic.py"
                      }
                      dir("characterise_c_py"){
                        // This test compares the suppression performance across angles between model and C implementation
                        // and fails if they differ significantly. It requires that the C implementation run with fixed mu
                        sh "pytest -s --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                        // This script sweeps the y_delay value to find what the optimum suppression is across RT60 and angle.
                        // It's more of a model develpment tool than testing the implementation so not run. It take a few minutes.
                        //sh "python sweep_ic_delay.py"
                      }
                      dir("test_calc_vnr_pred"){
                        // This is a unit test for ic_calc_vnr_pred function.
                        sh "pytest -n 2 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                      dir("test_bad_state"){
                        sh "pytest -s --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                    }
                  }
                }
              }
            }
          }
        }

        stage('Stage B tests') {
          steps {
            catchError(stageResult: 'FAILURE', catchInterruptions: false){
              dir("${REPO}/tests/stage_b") {
                withTools(params.TOOLS_VERSION) {
                  withVenv {
                    withEnv(["hydra_audio_PATH=/projects/hydra_audio"]) {
                      sh "pytest -n 1 --junitxml=pytest_result.xml"
                      junit "pytest_result.xml"
                    }
                  }
                }
              }
            }
          }
        }

        stage('ADEC tests') {
          steps {
            catchError(stageResult: 'FAILURE', catchInterruptions: false){
              dir("${REPO}/tests/lib_adec") {
                withTools(params.TOOLS_VERSION) {
                  withVenv {
                    withEnv(["hydra_audio_PATH=/projects/hydra_audio"]) {
                      dir("de_unit_tests") {
                        sh "pytest -n 2 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                      dir("test_delay_estimator") {
                        sh "pytest -n 2 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                        sh "python print_stats.py"
                      }
                      dir("test_adec_startup") {
                        sh "pytest -n 2 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                      dir("test_adec") {
                        sh "pytest -n 2 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                    }
                  }
                }
              }
            }
          }
        }

        stage('AEC tests') {
          steps {
            catchError(stageResult: 'FAILURE', catchInterruptions: false){
              dir("${REPO}/tests/lib_aec") {
                withTools(params.TOOLS_VERSION) {
                  withVenv {
                    withEnv(["hydra_audio_PATH=/projects/hydra_audio"]) {
                      dir("test_aec_schedule") {
                        sh "pytest -n 1 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                      dir("test_aec_enhancements") {
                        sh "pytest -n 2 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                      dir("aec_unit_tests") {
                        sh "pytest -n 2 --junitxml=pytest_result.xml"
                        junit "pytest_result.xml"
                      }
                      dir("test_aec_spec") {
                        script {
                          if (env.FULL_TEST == "0") {
                            sh 'mv excluded_tests_quick.txt excluded_tests.txt'
                          }
                        }
                        sh "python generate_audio.py"
                        sh "pytest -n 2 --junitxml=results_process.xml test_process_audio.py"
                        catchError {
                          sh "pytest --junitxml=results_check.xml test_check_output.py"
                        }
                        sh "python parse_results.py"
                        sh "pytest --junitxml=results_final.xml test_evaluate_results.py"
                        junit "results_final.xml"
                      }
                    }
                  }
                }
              }
            }
          }
        }

        stage('AGC tests') {
          steps {
            catchError(stageResult: 'FAILURE', catchInterruptions: false){
              dir("${REPO}/tests/lib_agc/test_process_frame") {
                withTools(params.TOOLS_VERSION) {
                  withVenv {
                    sh "pytest -n 2 --junitxml=pytest_result.xml"
                    junit "pytest_result.xml"
                  }
                }
              }
            }
          }
        }
        stage('Pipeline tests') {
          steps {
            catchError(stageResult: 'FAILURE', catchInterruptions: false){
              dir("${REPO}/tests/pipeline") {
                withEnv(["hydra_audio_PATH=/projects/hydra_audio"]) {
                  withEnv(["PIPELINE_FULL_RUN=${PIPELINE_FULL_RUN}", "SENSORY_PATH=${env.WORKSPACE}/sensory_sdk/", "AMAZON_WWE_PATH=${env.WORKSPACE}/amazon_wwe/"]) {
                    withTools(params.TOOLS_VERSION) {
                      withVenv {
                        echo "PIPELINE_FULL_RUN set as " + env.PIPELINE_FULL_RUN

                        // Note we have 2 xcore targets and we can run x86 threads too. But in case we have only xcore jobs in the config, limit to 4 so we don't timeout waiting for xtags
                        sh "pytest -n 4 --junitxml=pytest_result.xml -vv"
                        junit "pytest_result.xml"
                        sh "python compare_keywords.py results_Avona_aec_ic_ns_agc_prev_arch_xcore.csv results_Avona_aec_ic_ns_agc_prev_arch_python.csv --pass-threshold=1"
                      }
                    }
                  }
                }
              }
            }
          }
        }
        stage('Benchmark Pipeline tests results') {
          when {
            expression { env.PIPELINE_FULL_RUN == "1" }
          }
          steps {
            dir("${REPO}/tests/pipeline") {
              withTools(params.TOOLS_VERSION) {
                withVenv {
                  copyArtifacts filter: '**/results_*.csv', fingerprintArtifacts: true, projectName: '../lib_audio_pipelines/master', selector: lastSuccessful()
                  runPython("python plot_results.py lib_audio_pipelines/tests/pipelines/results_lib_ap_prev_arch_xcore.csv results_Avona_prev_arch_xcore.csv --single-plot --ww-column='0_2 1_2' --figname=results_benchmark_prev_arch")
                  runPython("python plot_results.py lib_audio_pipelines/tests/pipelines/results_lib_ap_alt_arch_xcore.csv results_Avona_alt_arch_xcore.csv --single-plot --ww-column='0_2 1_2' --figname=results_benchmark_alt_arch")
                }
              }
            }
          }
        }
      }// stages
      post {
        always {
          // IC artefacts
          archiveArtifacts artifacts: "${REPO}/tests/lib_ic/test_ic_spec/ic_spec_summary.txt", fingerprint: true
          // Pipelines tests
          archiveArtifacts artifacts: "${REPO}/tests/pipeline/**/results_*.csv", fingerprint: true
          archiveArtifacts artifacts: "${REPO}/tests/pipeline/**/results_*.png", fingerprint: true, allowEmptyArchive: true
          archiveArtifacts artifacts: "${REPO}/tests/pipeline/keyword_input_*/*.npy", fingerprint: true, allowEmptyArchive: true
        }
        failure {
          // archive wavs on failure only
          archiveArtifacts artifacts: "${REPO}/tests/pipeline/keyword_input_*/*.wav", fingerprint: true
        }
        cleanup {
          xcoreCleanSandbox()
        }
      }
    }// stage xcore.ai Verification

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
