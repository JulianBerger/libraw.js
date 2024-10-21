import { createDefaultEsmPreset } from 'ts-jest';

/** @type {import('ts-jest').JestConfigWithTsJest} **/
export default {
  testEnvironment: 'node',
  testMatch: ['<rootDir>/src/test/**/*.test.ts'],
  transform: {
    '^.+.tsx?$': [
      'ts-jest',
      {
        useESM: true,
        diagnostics: {
          ignoreCodes: [1343],
        },
        astTransformers: {
          before: [
            {
              path: './node_modules/ts-jest-mock-import-meta',
              options: {
                metaObjectReplacement: {
                  url: ({ fileName }) => `file://${fileName}`,
                  file: ({ fileName }) => fileName,
                },
              },
            },
          ],
        },
      },
    ],
  },
  extensionsToTreatAsEsm: ['.ts'],
  setupFiles: ['<rootDir>/src/test/test-setup.ts'],
  moduleFileExtensions: ['ts', 'js'],
  ...createDefaultEsmPreset({
    tsconfig: 'tsconfig.json',
  }),
  testTimeout: 100000,
};
