### imgui_dx11_blur

- how to use:

  ```c++
  //use this function after imgui initialization
  Blur::Initialize(device, context);
  
  ...


  //You need to use blur before adding widgets
  ImGui::Begin("Window Title", &g.showMenu, ImGuiWindowFlags_NoSavedSettings);
  Blur::Begin();
  if (g.blurBackground)
     Blur::Apply(ImGui::GetBackgroundDrawList(), ImGui::GetMainViewport()->Pos, ImGui::GetMainViewport()->Size, g.radius);
  else
     Blur::Apply(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), ImGui::GetWindowSize(), g.radius);
  
  ImGui::Checkbox("Switch to background blur", &g.blurBackground);
  ImGui::SliderFloat("Radius", &g.radius, 0.0f, 10.0f, "Radius = %.1f");
  Blur::End();
  ImGui::End();
  ```

- enjoy:
  <img width="2161" height="1564" alt="image" src="https://github.com/user-attachments/assets/5d121276-1b92-4d19-bf75-845d82090ce1" />
