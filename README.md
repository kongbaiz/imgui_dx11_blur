### imgui_dx11_blur

- how to use:

  ```c++
  //use this function after imgui initialization
  Blur::Initialize(device, context);
  
  ...

  //You need to use blur before adding widgets
  static bool showMenu = false;
  static bool blurBackground = false;
  static float radius = 4.0f;
  
  ImGui::Begin("Window Title", &showMenu, ImGuiWindowFlags_NoSavedSettings);
  {
    
    Blur::Begin();
    {
      if (blurBackground)
         Blur::Apply(ImGui::GetBackgroundDrawList(), ImGui::GetMainViewport()->Pos, ImGui::GetMainViewport()->Size, radius);
      else
         Blur::Apply(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), ImGui::GetWindowSize(), radius);
    }
    Blur::End();
    
    ImGui::Checkbox("Switch to background blur", &blurBackground);
    ImGui::SliderFloat("Radius", &radius, 0.0f, 10.0f, "Radius = %.1f");

  }
  ImGui::End();
  ```

- enjoy:
  <img width="2161" height="1564" alt="image" src="https://github.com/user-attachments/assets/5d121276-1b92-4d19-bf75-845d82090ce1" />
