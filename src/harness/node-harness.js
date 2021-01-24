/*
 * Copyright 2020 Sam Thorogood.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

/**
 * @fileoverview Node wrapper for Blep. Uses Node's fs package to load the runner wasm.
 */

import * as blep from './types/index.js';

export * from './harness.js';
import build from './harness.js';

import * as fs from 'fs';

/**
 * @return {!Promise<blep.Harness>}
 */
export default async function wrapper() {
  const {pathname} = new URL('./runner.wasm', import.meta.url);
  const wasm = fs.readFileSync(pathname);
  return build(Promise.resolve(wasm));
}
